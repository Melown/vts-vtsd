/**
 * Copyright (c) 2017 Melown Technologies SE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * *  Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>

#include <opencv2/highgui/highgui.hpp>

#include "dbglog/dbglog.hpp"

#include "http/error.hpp"

#include "./sink.hpp"
#include "./error.hpp"

namespace {

boost::optional<long> maxAge(FileClass fileClass
                             , const FileClassSettings *fileClassSettings)
{
    if (!fileClassSettings) { return boost::none; }
    return fileClassSettings->getMaxAge(fileClass);
}

Sink::FileInfo update(const Sink::FileInfo &inStat
                      , const FileClassSettings *fileClassSettings)
{
    if (inStat.maxAge) { return inStat; }
    auto stat(inStat);

    if (!fileClassSettings) {
        // no file class attached, no caching
        stat.maxAge = -1;
        return stat;
    }

    // set max age based on fileClass settings
    stat.maxAge = fileClassSettings->getMaxAge(stat.fileClass);

    // done
    return stat;
}

class IStreamDataSource : public http::ServerSink::DataSource {
public:
    IStreamDataSource(const vs::IStream::pointer &stream
                      , FileClass fileClass
                      , const FileClassSettings *fileClassSettings)
        : stream_(stream), stat_(stream->stat())
        , fs_(Sink::FileInfo(stat_.contentType, stat_.lastModified
                             , maxAge(fileClass, fileClassSettings)))
    {
        // do not fail on eof
        stream->get().exceptions(std::ios::badbit);
    }

    virtual http::SinkBase::FileInfo stat() const {
        return fs_;
    }

    virtual std::size_t read(char *buf, std::size_t size
                                 , std::size_t off)
    {
        return stream_->read(buf, size, off);
    }

    virtual std::string name() const { return stream_->name(); }

    virtual void close() const { stream_->close(); }

    virtual long size() const { return stat_.size; }

    virtual const http::Header::list *headers() const { return &headers_; }

private:
    vs::IStream::pointer stream_;
    vs::FileStat stat_;
    Sink::FileInfo fs_;
    http::Header::list headers_;
};

class SubIStreamDataSource : public http::ServerSink::DataSource {
public:
    SubIStreamDataSource(const vs::IStream::pointer &stream
                         , FileClass fileClass
                         , const FileClassSettings *fileClassSettings
                         , std::size_t offset, std::size_t size
                         , bool gzipped)
        : stream_(stream), stat_(stream->stat())
        , fs_(Sink::FileInfo(stat_.contentType, stat_.lastModified
                             , maxAge(fileClass, fileClassSettings)))
        , offset_(offset), end_(offset + size)
    {
        // sanity check
        if (end_ > stat_.size) { end_ = stat_.size; }

        // update size
        stat_.size = (end_ - offset_);

        // do not fail on eof
        stream->get().exceptions(std::ios::badbit);

        if (gzipped) {
            headers_.emplace_back("Content-Encoding", "gzip");
        }
    }

    virtual http::SinkBase::FileInfo stat() const { return fs_; }

    virtual std::size_t read(char *buf, std::size_t size, std::size_t off) {
        // fix-ups
        auto offset(off + offset_);
        if (offset > end_) { return 0; }
        auto left(end_ - offset);
        if (size > left) { size = left; }
        return stream_->read(buf, size, offset);
    }

    virtual std::string name() const { return stream_->name(); }

    virtual void close() const { stream_->close(); }

    virtual long size() const { return stat_.size; }

    virtual const http::Header::list *headers() const { return &headers_; }

private:
    vs::IStream::pointer stream_;
    vs::FileStat stat_;
    Sink::FileInfo fs_;
    http::Header::list headers_;
    std::size_t offset_;
    std::size_t end_;
};

class RoArchiveDataSource : public http::SinkBase::DataSource
{
public:
    RoArchiveDataSource(const roarchive::IStream::pointer &is
                        , const std::string &contentType, FileClass fileClass
                        , const FileClassSettings *fileClassSettings
                        , bool gzipped)
        : http::SinkBase::DataSource(true), is_(is)
        , size_(is_->size() ? *is_->size() : -1)
        , seekable_(is_->seekable()), off_()
    {
        fi_.lastModified = is_->timestamp();
        fi_.contentType = contentType;
        fi_.maxAge = maxAge(fileClass, fileClassSettings);

        if (gzipped) {
            headers_.emplace_back("Content-Encoding", "gzip");
        }
    }

private:
    virtual http::SinkBase::FileInfo stat() const { return fi_; }

    virtual std::size_t read(char *buf, std::size_t size
                             , std::size_t off);

    virtual void close() const { is_->close(); }

    virtual long size() const { return size_; }

    virtual const http::Header::list *headers() const { return &headers_; }

    roarchive::IStream::pointer is_;
    http::SinkBase::FileInfo fi_;

    long size_;
    bool seekable_;
    std::size_t off_;
    http::Header::list headers_;
};

std::size_t RoArchiveDataSource::read(char *buf, std::size_t size
                                      , std::size_t off)
{
    if (off != off_) {
        if (seekable_) {
            is_->get().seekg(off);
            off_ = off;
        } else {
            LOGTHROW(err2, std::runtime_error)
                << "This archive file is unseekable.";
        }
    }

    // fix size if too long
    {
        long end(off_ + size);
        if (end > size_) { size = size_ - off_; }
    }

    is_->get().read(buf, size);
    auto read(is_->get().gcount());

    off_ = off + read;
    return read;
}

} // namespace

void Sink::content(const vs::IStream::pointer &stream
                   , FileClass fileClass)
{
    sink_->content(std::make_shared<IStreamDataSource>
                   (stream, fileClass, &locationConfig_.fileClassSettings));
}

void Sink::content(const vs::IStream::pointer &stream
                   , FileClass fileClass, std::size_t offset, std::size_t size
                   , bool gzipped)
{
    sink_->content(std::make_shared<SubIStreamDataSource>
                   (stream, fileClass, &locationConfig_.fileClassSettings
                    , offset, size, gzipped));
}

void Sink::content(const roarchive::IStream::pointer &stream
                   , const std::string &contentType
                   , FileClass fileClass, bool gzipped)
{
    sink_->content(std::make_shared<RoArchiveDataSource>
                   (stream, contentType
                    , fileClass, &locationConfig_.fileClassSettings
                    , gzipped));
}

void Sink::content(const vs::SupportFile &data)
{
    if (!data.isTemplate) {
        FileInfo stat(data.contentType, data.lastModified);
        stat.setFileClass(FileClass::support);
        // not a template
        content(data.data, data.size, stat, false);
        return;
    }

    // content is expanded -> modified now!
    FileInfo stat(data.contentType);
    stat.setFileClass(FileClass::support);
    content(data.expand(&locationConfig_.vars, nullptr), stat);
}

void Sink::error(const std::exception_ptr &exc)
{
    sink_->error(exc);
}

Sink::FileInfo& Sink::FileInfo::setFileClass(FileClass fc)
{
    fileClass = fc;
    return *this;
}

Sink::FileInfo& Sink::FileInfo::setMaxAge(const boost::optional<long> &ma)
{
    maxAge = ma;
    return *this;
}

Sink::FileInfo Sink::update(const FileInfo &stat) const
{
    return ::update(stat, &locationConfig_.fileClassSettings);
}

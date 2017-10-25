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

#include <mutex>

#include <boost/optional.hpp>
#include <boost/utility/in_place_factory.hpp>
#include <boost/algorithm/string/find.hpp>

#include "utility/uri.hpp"

#include "slpk/reader.hpp"

#include "vts-libs/vts/support.hpp"
#include "vts-libs/vts.hpp"
#include "vts-libs/vts/tileop.hpp"
#include "vts-libs/vts/tileset/driver.hpp"
#include "vts-libs/vts/tileset/delivery.hpp"
#include "vts-libs/storage/fstreams.hpp"
#include "vts-libs/vts/2d.hpp"
#include "vts-libs/vts/debug.hpp"
#include "vts-libs/vts/virtualsurface.hpp"

#include "./driver.hpp"

namespace fs = boost::filesystem;
namespace ba = boost::algorithm;

namespace {

class SlpkDriver : public DriverWrapper
{
public:
    SlpkDriver(slpk::Archive &&reader)
        : DriverWrapper(DatasetProvider::slpk)
        , reader_(std::move(reader))
    {
        // build scene server info
        std::tie(sli_, sceneServerConfig_) = reader_.sceneServerConfig();

        // build layer prefix
        layerPrefix_
            = utility::Uri::joinAndRemoveDotSegments("/", sli_.href)
            .substr(1);

        LOG(info4) << "layerPrefix: <" << layerPrefix_ << ">.";
    }

    virtual vs::Resources resources() const {
        return { 1, 0 };
    }

    virtual bool externallyChanged() const { return false; }

    virtual void handle(Sink sink, const std::string &path
                        , const LocationConfig &config);

private:
    slpk::Archive reader_;
    std::string name_;

    slpk::SceneLayerInfo sli_;
    std::string sceneServerConfig_;
    std::string layerPrefix_;
};

void SlpkDriver::handle(Sink sink, const std::string &path
                        , const LocationConfig &config)
{
    (void) config;

    if (path == ".") {
        // scene server config
        return sink.content
            (sceneServerConfig_
             , Sink::FileInfo("application/json", -1)
             .setFileClass(FileClass::config));
    }

    if (!ba::starts_with(path, layerPrefix_)) {
        return sink.error(utility::makeError<NotFound>("Unknown file."));
    }

    auto localPath(path.substr(layerPrefix_.size()));

    LOG(info4) << "localPath: <" << localPath << ">.";

    auto is(reader_.rawistream(path));

    // TODO: detect content type

    // TODO: detect gzip encoding by peeking at first byte
    //       -> Transfer-Encoding

    return sink.content(is, "application/octet-stream", FileClass::data);
}

} // namespace

DriverWrapper::pointer openSlpk(const OpenInfo &openInfo
                               , const OpenOptions&
                               , DeliveryCache &cache
                               , const DeliveryCache::Callback &callback)
{
    if (openInfo.mime != "application/zip") {
        throw vs::NoSuchTileSet(openInfo.path.string());
    }

    cache.post(callback, [=]() -> void
    {
        // TODO: handle exceptions

        slpk::Archive reader
            (openInfo.path, openInfo.mime);
        callback(DriverWrapper::pointer
                 (std::make_shared<SlpkDriver>(std::move(reader))));
    });
    return {};
}

namespace {
std::array<std::string, 2> extensions{{ ".slpk", ".spk" }};
} // namespace constant

/** Tries to find *.slpk or *.spk filename in the path and split it after this
  * filename if found.
 */
bool slpkSplitFilePath(const boost::filesystem::path &filePath, SplitPath &sp)
{
    const auto &path(filePath.string());
    const auto begin(path.begin());
    const auto end(path.end());

    for (const auto &ext : extensions) {
        auto range(ba::ifind_first(path, ext));
        if (std::begin(range) == std::end(range)) { continue; }

        auto erange(std::end(range));
        auto eerange(erange);

        if ((erange != end) && (*erange == '/')) { ++eerange; }
        if (eerange == end) {
            sp.first = filePath.parent_path();
            sp.second = filePath.filename();
        } else {
            // keep slast at the start of file
            sp.first = std::string(begin, erange);
            sp.second = std::string(eerange, end);
        }
        return true;
    }

    return false;
}

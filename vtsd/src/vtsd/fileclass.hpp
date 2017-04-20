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

#ifndef httpd_fileclass_hpp_included_
#define httpd_fileclass_hpp_included_

#include <array>

#include <boost/any.hpp>
#include <boost/program_options.hpp>

#include "utility/enum-io.hpp"

/** If adding into this enum leave unknown the last one!
 *  Make no holes, i.e. we can use values directly as indices to an array
 */
enum class FileClass { config, support, registry, data, unknown };

class FileClassSettings {
public:
    FileClassSettings() : maxAges_{{0}} {
        // unknown files are never cached -- for example directory listings
        setMaxAge(FileClass::unknown, -1);
    }

    void configuration(boost::program_options::options_description &od
                       , const std::string &prefix = "");

    void setMaxAge(FileClass fc, long value);
    long getMaxAge(FileClass fc) const;

    void dump(std::ostream &os, const std::string &prefix) const;

private:
    std::array<long, static_cast<int>(FileClass::unknown) + 1> maxAges_;
};

UTILITY_GENERATE_ENUM_IO(FileClass,
                         ((config))
                         ((support))
                         ((registry))
                         ((data))
                         ((unknown))
                         )

// inlines

inline void FileClassSettings::setMaxAge(FileClass fc, long value)
{
    maxAges_[static_cast<int>(fc)] = value;
}

inline long FileClassSettings::getMaxAge(FileClass fc) const
{
    return maxAges_[static_cast<int>(fc)];
}

#endif // httpd_fileclass_hpp_included_

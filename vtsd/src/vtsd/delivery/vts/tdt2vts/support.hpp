/**
 * Copyright (c) 2020 Melown Technologies SE
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

#ifndef vtsd_delivery_vts_vts2tdt_support_hpp_included_
#define vtsd_delivery_vts_vts2tdt_support_hpp_included_

#include "vts-libs/storage/support.hpp"
#include "vts-libs/vts/basetypes.hpp"

namespace vts2tdt {

/** Compiled-in support files (browser etc).
 */
extern const vtslibs::storage::SupportFile::Files supportFiles;

/** Default variables for compiled-in support files.
 */
extern const vtslibs::storage::SupportFile::Vars defaultSupportVars;

/** Build filename for tile tile
 */
std::string filename(const vtslibs::vts::TileId &tileId
                     , const std::string &ext
                     , const boost::optional<int> &sub = boost::none);

// inlines

inline std::string filename(const vtslibs::vts::TileId &tileId
                            , const std::string &ext
                            , const boost::optional<int> &sub)
{
    std::ostringstream os;
    os << tileId.lod << '-' << tileId.x << '-' << tileId.y;
    if (sub) { os << '-' << *sub; }
    os << '.' << ext;
    return os.str();
}

} // namespace vts2tdt

#endif // vtsd_delivery_vts_vts2tdt_support_hpp_included_

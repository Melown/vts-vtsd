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

#include "utility/rlimit.hpp"

#include "vts-libs/storage/error.hpp"
#include "vts-libs/storage/io.hpp"

#include "./cache.hpp"
#include "./vts/driver.hpp"
#include "./tileset/driver.hpp"
#include "./vts0/driver.hpp"

/** Time between flushes.
 */
constexpr std::time_t FLUSH_INTERVAL(60);

/** Maximal time between hits in cache for single record.
 */
constexpr std::time_t MAX_INTERVAL_BETWEEN_HITS(600);

DeliveryCache::DeliveryCache()
    : nextFlush_(std::time(nullptr) + FLUSH_INTERVAL)
{
    cleanupLimit_.openFiles = utility::maxOpenFiles() / 2;
    cleanupLimit_.memory
        = std::numeric_limits<decltype(cleanupLimit_.memory)>::max();

    LOG(info3) << "Cleanup limits: " << cleanupLimit_ << ".";
}

DriverWrapper::pointer DeliveryCache::openDriver
(const std::string &path, const vtslibs::vts::OpenOptions &openOptions) const
{
    // try VTS
    try { return openVts(path, openOptions); } catch (vs::NoSuchTileSet) {}

    // try VTS0
    try { return openVts0(path); } catch (vs::NoSuchTileSet) {}

    // finaly, try old TS
    return openTileSet(path);
}

DriverWrapper::pointer
DeliveryCache::get(const std::string &path
                   , const vtslibs::vts::OpenOptions &openOptions)
{
    LOG(info1) << "Getting driver for tileset at: \"" << path << "\".";

    std::unique_lock<std::mutex> guard(mutex_);

    // clean resource hoggers and flush changed tile sets
    cleanup(guard);
    flush(guard);

    // TODO: use just path when updated
    auto fdrivers(drivers_.find(boost::make_tuple(path, 0)));

    bool replace(false);

    if (fdrivers != drivers_.end()) {
        auto driver(fdrivers->driver);
        replace = (driver->hotContent() && driver->externallyChanged());

        if (!replace) {
            // modify record
            // TODO: what if this fails (can it fail???)
            drivers_.modify(fdrivers, [](Record &r) { r.update(); });
            return fdrivers->driver;
        }
    }

    // open driver
    auto driver(openDriver(path, openOptions));

    if (replace) {
        // replace existing driver (it doesn't contribute to the key)
        fdrivers->driver = driver;
    } else {
        // cache new record
        drivers_.insert(Record(path, driver, totalResources_));
    }

    // done
    return driver;
}

void DeliveryCache::cleanup()
{
    std::unique_lock<std::mutex> guard(mutex_);
    cleanup(guard);
    flush(guard);
}

void DeliveryCache::cleanup(std::unique_lock<std::mutex>&)
{
    if (totalResources_ < cleanupLimit_) { return; }

    LOG(info2) << "Resource limit reached (total: " << totalResources_
               << " >= limit " << cleanupLimit_ << ".";

    auto &idx(drivers_.get<ResourcesIdx>());

    for (auto iidx(idx.begin());
         (totalResources_ < cleanupLimit_) && (iidx != idx.end()); )
    {
        const auto &r(*iidx);
        LOG(info1) << "Removing cached tileset <" << r.path << "> "
                   << "with resources " << r.resources << ".";
        iidx = idx.erase(iidx);
    }
}

void DeliveryCache::flush(std::unique_lock<std::mutex>&)
{
    const auto now(std::time(nullptr));
    if (nextFlush_ > now) { return; }

    const auto killHit(now - MAX_INTERVAL_BETWEEN_HITS);

    for (auto iidx(drivers_.begin()); (iidx != drivers_.end()); ) {
        const auto &r(*iidx);
        // initialize remove flag based on the last hit
        bool remove(r.lastHit < killHit);
        if (!remove) {
            try {
                remove = r.driver->externallyChanged();
            } catch (const std::exception &e) {
                LOG(warn2) << "External change test failed: " << e.what()
                           << "; removing driver.";
                remove = true;
            } catch (...) {
                LOG(warn2)
                    << "External change test failed (unknown error);"
                    << " removing driver.";
                remove = true;
            }
        }

        if (remove) {
            LOG(info1) << "Removing cached tileset <" << r.path << "> "
                       << " that has been externally changed or timed out.";
            iidx = drivers_.erase(iidx);
        } else {
            ++iidx;
        }
    }

    nextFlush_ = now + FLUSH_INTERVAL;
}

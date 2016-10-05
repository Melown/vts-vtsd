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

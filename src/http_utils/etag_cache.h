#ifndef ETAG_CACHE_H
#define ETAG_CACHE_H
#include <sys/stat.h>

#include <map>
#include <mutex>
#include <string>

class ETagCache
{
public:
    static ETagCache& instance();
    /* returns a quoted strong etag, or an empty string on failure. */
    std::string etagFor(const std::string& path, const struct stat& st);

private:
    ETagCache();

    struct Entry
    {
        long long size;
        long long stamp; /* st_mtim in nanoseconds; second-only granularity hides same-second rewrites */
        std::string etag;
    };

    std::mutex mutex_;
    std::map<std::string, Entry> entries_;

    static bool hashFile(const std::string& path, std::string& out);
};

#endif

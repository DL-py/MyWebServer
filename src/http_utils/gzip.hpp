#ifndef GZIP_H
#define GZIP_H

#include <cstddef>
#include <map>
#include <mutex>
#include <string>

/* gzip-compress data into out. returns false on zlib error. */
bool gzipCompress(const char* data, std::size_t length, std::string& out);

/* true when the Accept-Encoding field value selects the gzip coding. */
bool acceptsGzipEncoding(const std::string& header);

/* bounded, process-local cache of gzip-compressed bodies. */
class GzipCache
{
public:
    static GzipCache& instance();

    void setBudget(std::size_t bytes);
    /* true when a compressed body for path+size+stamp is cached; result is copied into out. */
    bool get(const std::string& path, long long size, long long stamp, std::string& out);
    void put(const std::string& path, long long size, long long stamp, const std::string& compressed);

    /* remembers that compressing this entity did not pay off, so revalidation
       takes the identity path and can still answer 304. */
    bool isCompressionNotBeneficial(const std::string& path, long long size, long long stamp);
    void markCompressionNotBeneficial(const std::string& path, long long size, long long stamp);

private:
    GzipCache();

    struct Entry
    {
        long long size;
        long long stamp;
        long long seq;
        std::size_t bytes;
        std::string data;
    };

    struct Marker
    {
        long long size;
        long long stamp;
        long long seq;
    };

    std::mutex mutex_;
    std::map<std::string, Entry> entries_;
    std::map<std::string, Marker> markers_;
    std::size_t total_;
    std::size_t budget_;
    long long seq_;
};

#endif

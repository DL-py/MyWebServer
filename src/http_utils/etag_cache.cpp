#include "etag_cache.h"

#include <cstdint>
#include <cstdio>

namespace
{
const std::uint64_t FNV_OFFSET = 14695981039346656037ULL;
const std::uint64_t FNV_PRIME = 1099511628211ULL;
const std::size_t CHUNK_SIZE = 64 * 1024;
}  // namespace

ETagCache::ETagCache()
{
}

ETagCache& ETagCache::instance()
{
    static ETagCache cache;
    return cache;
}

bool ETagCache::hashFile(const std::string& path, std::string& out)
{
    FILE* file = fopen(path.c_str(), "rb");
    if (file == nullptr)
    {
        return false;
    }

    std::uint64_t hash = FNV_OFFSET;
    unsigned char buffer[CHUNK_SIZE];
    while (true)
    {
        const std::size_t readSize = fread(buffer, 1, CHUNK_SIZE, file);
        for (std::size_t i = 0; i < readSize; ++i)
        {
            hash ^= static_cast<std::uint64_t>(buffer[i]);
            hash *= FNV_PRIME;
        }
        if (readSize < CHUNK_SIZE)
        {
            break;
        }
    }
    const bool ok = ferror(file) == 0;
    fclose(file);
    if (!ok)
    {
        return false;
    }

    char hex[32];
    snprintf(hex, sizeof(hex), "\"%016llx\"", static_cast<unsigned long long>(hash));
    out = hex;
    return true;
}

std::string ETagCache::etagFor(const std::string& path, const struct stat& st)
{
    const long long size = static_cast<long long>(st.st_size);
    const long long stamp = static_cast<long long>(st.st_mtim.tv_sec) * 1000000000LL +
                            static_cast<long long>(st.st_mtim.tv_nsec);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(path);
        if (it != entries_.end() && it->second.size == size && it->second.stamp == stamp)
        {
            return it->second.etag;
        }
    }

    std::string etag;
    if (!hashFile(path, etag))
    {
        return "";
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        Entry entry;
        entry.size = size;
        entry.stamp = stamp;
        entry.etag = etag;
        entries_[path] = entry;
    }
    return etag;
}

#include "gzip.hpp"

#include <cstdlib>
#include <cstring>

#include <strings.h>
#include <zlib.h>

GzipCache::GzipCache()
    : total_(0), budget_(32 * 1024 * 1024), seq_(0)
{
}

GzipCache& GzipCache::instance()
{
    static GzipCache cache;
    return cache;
}

bool gzipCompress(const char* data, std::size_t length, std::string& out)
{
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    if (deflateInit2(&stream, 6, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
    {
        return false;
    }

    std::string buffer;
    buffer.resize(length / 2 + 64);
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data));
    stream.avail_in = static_cast<uInt>(length);
    stream.next_out = reinterpret_cast<Bytef*>(&buffer[0]);
    stream.avail_out = static_cast<uInt>(buffer.size());

    int result = Z_OK;
    while (true)
    {
        result = deflate(&stream, Z_FINISH);
        if (result == Z_STREAM_END)
        {
            break;
        }
        if (result != Z_OK && result != Z_BUF_ERROR)
        {
            deflateEnd(&stream);
            return false;
        }
        if (stream.avail_out == 0)
        {
            const std::size_t used = buffer.size();
            buffer.resize(used * 2);
            stream.next_out = reinterpret_cast<Bytef*>(&buffer[0] + used);
            stream.avail_out = static_cast<uInt>(buffer.size() - used);
            continue;
        }
        if (result == Z_BUF_ERROR)
        {
            /* no progress possible and output space is left: zlib failed. */
            deflateEnd(&stream);
            return false;
        }
    }

    buffer.resize(stream.total_out);
    deflateEnd(&stream);
    out = buffer;
    return true;
}

bool acceptsGzipEncoding(const std::string& header)
{
    if (header.empty())
    {
        return false;
    }

    std::string trimmed = header;
    while (!trimmed.empty() && (trimmed[0] == ' ' || trimmed[0] == '\t'))
    {
        trimmed.erase(trimmed.begin());
    }
    while (!trimmed.empty() &&
           (trimmed[trimmed.size() - 1] == ' ' || trimmed[trimmed.size() - 1] == '\t'))
    {
        trimmed.erase(trimmed.size() - 1);
    }

    std::size_t start = 0;
    while (start <= trimmed.size())
    {
        const std::size_t comma = trimmed.find(',', start);
        std::string token = trimmed.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        while (!token.empty() && (token[0] == ' ' || token[0] == '\t'))
        {
            token.erase(token.begin());
        }
        while (!token.empty() &&
               (token[token.size() - 1] == ' ' || token[token.size() - 1] == '\t'))
        {
            token.erase(token.size() - 1);
        }

        std::string coding = token;
        double quality = 1.0;
        const std::size_t semicolon = token.find(';');
        if (semicolon != std::string::npos)
        {
            coding = token.substr(0, semicolon);
            const std::string parameters = token.substr(semicolon + 1);
            for (std::size_t i = 0; i + 1 < parameters.size(); ++i)
            {
                if ((parameters[i] == 'q' || parameters[i] == 'Q') &&
                    parameters[i + 1] == '=')
                {
                    quality = strtod(parameters.c_str() + i + 2, nullptr);
                    break;
                }
            }
        }
        while (!coding.empty() &&
               (coding[coding.size() - 1] == ' ' || coding[coding.size() - 1] == '\t'))
        {
            coding.erase(coding.size() - 1);
        }

        if ((strcasecmp(coding.c_str(), "gzip") == 0 ||
             strcasecmp(coding.c_str(), "x-gzip") == 0 || coding == "*") &&
            quality > 0.0)
        {
            return true;
        }
        if (comma == std::string::npos)
        {
            break;
        }
        start = comma + 1;
    }
    return false;
}

void GzipCache::setBudget(std::size_t bytes)
{
    std::lock_guard<std::mutex> lock(mutex_);
    budget_ = bytes;
    while (total_ > budget_ && !entries_.empty())
    {
        auto oldest = entries_.begin();
        for (auto it = entries_.begin(); it != entries_.end(); ++it)
        {
            if (it->second.seq < oldest->second.seq)
            {
                oldest = it;
            }
        }
        total_ -= oldest->second.bytes;
        entries_.erase(oldest);
    }
}

bool GzipCache::get(const std::string& path, long long size, long long stamp, std::string& out)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(path);
    if (it == entries_.end() || it->second.size != size || it->second.stamp != stamp)
    {
        return false;
    }
    out = it->second.data;
    return true;
}

void GzipCache::put(const std::string& path, long long size, long long stamp,
                    const std::string& compressed)
{
    std::lock_guard<std::mutex> lock(mutex_);

    /* an entry that alone exceeds the budget would flush everything and be
       evicted itself; refuse it instead of thrashing the cache. */
    if (compressed.size() > budget_)
    {
        return;
    }

    auto existing = entries_.find(path);
    if (existing != entries_.end())
    {
        total_ -= existing->second.bytes;
        entries_.erase(existing);
    }

    Entry entry;
    entry.size = size;
    entry.stamp = stamp;
    entry.seq = seq_++;
    entry.bytes = compressed.size();
    entry.data = compressed;
    total_ += entry.bytes;
    entries_[path] = entry;

    while (total_ > budget_ && !entries_.empty())
    {
        auto oldest = entries_.begin();
        for (auto it = entries_.begin(); it != entries_.end(); ++it)
        {
            if (it->second.seq < oldest->second.seq)
            {
                oldest = it;
            }
        }
        total_ -= oldest->second.bytes;
        entries_.erase(oldest);
    }
}

bool GzipCache::isCompressionNotBeneficial(const std::string& path, long long size,
                                           long long stamp)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = markers_.find(path);
    return it != markers_.end() && it->second.size == size && it->second.stamp == stamp;
}

void GzipCache::markCompressionNotBeneficial(const std::string& path, long long size,
                                             long long stamp)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (markers_.size() >= 4096)
    {
        auto oldest = markers_.begin();
        for (auto it = markers_.begin(); it != markers_.end(); ++it)
        {
            if (it->second.seq < oldest->second.seq)
            {
                oldest = it;
            }
        }
        markers_.erase(oldest);
    }
    Marker marker;
    marker.size = size;
    marker.stamp = stamp;
    marker.seq = seq_++;
    markers_[path] = marker;
}

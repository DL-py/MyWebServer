#include "session_store.h"

#include <openssl/rand.h>

#include <stdexcept>

namespace
{

const std::size_t TOKEN_SIZE = 32;

std::string toHex(const unsigned char* data, std::size_t size)
{
    static const char* const HEX_DIGITS = "0123456789abcdef";
    std::string out;
    out.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i)
    {
        out.push_back(HEX_DIGITS[data[i] >> 4]);
        out.push_back(HEX_DIGITS[data[i] & 0x0f]);
    }
    return out;
}

std::string generateToken()
{
    unsigned char raw[TOKEN_SIZE];
    if (RAND_bytes(raw, static_cast<int>(TOKEN_SIZE)) != 1)
    {
        throw std::runtime_error("RAND_bytes failed");
    }
    return toHex(raw, TOKEN_SIZE);
}

} // namespace

SessionStore::SessionStore(int defaultTtlSeconds)
    : defaultTtlSeconds_(defaultTtlSeconds)
{
}

std::string SessionStore::create(int userId, int ttlSeconds)
{
    const int ttl = (ttlSeconds == -1) ? defaultTtlSeconds_ : ttlSeconds;
    const std::string token = generateToken();

    Session session;
    session.userId = userId;
    session.expiresAt = std::time(nullptr) + ttl;

    std::lock_guard<std::mutex> lock(mutex_);
    sessions_[token] = session;
    return token;
}

bool SessionStore::get(const std::string& token, Session& out) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_map<std::string, Session>::iterator it = sessions_.find(token);
    if (it == sessions_.end())
    {
        return false;
    }
    if (it->second.expiresAt <= std::time(nullptr))
    {
        sessions_.erase(it);
        return false;
    }
    out = it->second;
    return true;
}

void SessionStore::remove(const std::string& token)
{
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(token);
}

void SessionStore::purgeExpired()
{
    std::lock_guard<std::mutex> lock(mutex_);
    const std::time_t now = std::time(nullptr);
    std::unordered_map<std::string, Session>::iterator it = sessions_.begin();
    while (it != sessions_.end())
    {
        if (it->second.expiresAt <= now)
        {
            it = sessions_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

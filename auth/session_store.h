#ifndef AUTH_SESSION_STORE_H
#define AUTH_SESSION_STORE_H
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_map>

struct Session
{
    int userId;
    std::time_t expiresAt;
};

class SessionStore
{
public:
    explicit SessionStore(int defaultTtlSeconds = 86400);

    std::string create(int userId, int ttlSeconds = -1);
    bool get(const std::string& token, Session& out) const;
    void remove(const std::string& token);
    void purgeExpired();

private:
    int defaultTtlSeconds_;
    mutable std::mutex mutex_;
    // Mutable so that get() can drop expired entries while staying const.
    mutable std::unordered_map<std::string, Session> sessions_;
};
#endif

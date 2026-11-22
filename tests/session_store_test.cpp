#include "../auth/session_store.h"
#include <iostream>

int main()
{
    SessionStore store(60);
    std::string t = store.create(42);
    if (t.size() != 64)
    {
        std::cerr << "FAIL: token length\n";
        return 1;
    }
    Session s;
    if (!store.get(t, s) || s.userId != 42)
    {
        std::cerr << "FAIL: get\n";
        return 1;
    }
    store.remove(t);
    if (store.get(t, s))
    {
        std::cerr << "FAIL: removed token still valid\n";
        return 1;
    }

    std::string expired = store.create(7, 0);
    if (store.get(expired, s))
    {
        std::cerr << "FAIL: expired token accepted\n";
        return 1;
    }
    if (store.create(1) == store.create(2))
    {
        std::cerr << "FAIL: tokens should differ\n";
        return 1;
    }

    std::cout << "session store test PASS\n";
    return 0;
}

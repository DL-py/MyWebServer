#include "../auth/password_hash.h"
#include <iostream>

int main()
{
    std::vector<unsigned char> salt(16, 0x42);
    PasswordHash a = hashPasswordWithSalt("Passw0rd!", salt, 1000);
    PasswordHash b = hashPasswordWithSalt("Passw0rd!", salt, 1000);
    if (a.hash != b.hash || a.hash.size() != 32 || a.algo != "pbkdf2-sha256")
    {
        std::cerr << "FAIL: deterministic hash\n";
        return 1;
    }
    if (!verifyPassword("Passw0rd!", a) || verifyPassword("wrong", a))
    {
        std::cerr << "FAIL: verify\n";
        return 1;
    }
    PasswordHash badAlgo = a;
    badAlgo.algo = "sha256";
    PasswordHash badIter = a;
    badIter.iterations = 0;
    if (verifyPassword("Passw0rd!", badAlgo) || verifyPassword("Passw0rd!", badIter))
    {
        std::cerr << "FAIL: unsupported stored parameters accepted\n";
        return 1;
    }

    PasswordHash c = hashPassword("Passw0rd!");
    if (c.salt.size() != 16 || c.hash.size() != 32 || c.iterations != 100000)
    {
        std::cerr << "FAIL: default sizes\n";
        return 1;
    }
    if (!verifyPassword("Passw0rd!", c))
    {
        std::cerr << "FAIL: verify random salt\n";
        return 1;
    }
    if (hashPassword("Passw0rd!").hash == hashPassword("Passw0rd!").hash)
    {
        std::cerr << "FAIL: random salts should differ\n";
        return 1;
    }

    std::cout << "password hash test PASS\n";
    return 0;
}

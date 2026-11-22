#ifndef AUTH_PASSWORD_HASH_H
#define AUTH_PASSWORD_HASH_H
#include <string>
#include <vector>

struct PasswordHash
{
    std::vector<unsigned char> salt;
    std::vector<unsigned char> hash;
    int iterations;
    std::string algo;
};

PasswordHash hashPasswordWithSalt(const std::string& password,
                                  const std::vector<unsigned char>& salt,
                                  int iterations);
PasswordHash hashPassword(const std::string& password);
bool verifyPassword(const std::string& password, const PasswordHash& stored);
#endif

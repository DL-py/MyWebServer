#include "password_hash.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <stdexcept>

namespace
{

const std::size_t SALT_SIZE = 16;
const std::size_t HASH_SIZE = 32;
const int DEFAULT_ITERATIONS = 100000;
const char* const ALGO_NAME = "pbkdf2-sha256";

std::vector<unsigned char> deriveHash(const std::string& password,
                                      const std::vector<unsigned char>& salt,
                                      int iterations)
{
    if (iterations <= 0 || salt.empty())
    {
        throw std::runtime_error("invalid pbkdf2 parameters");
    }

    std::vector<unsigned char> out(HASH_SIZE, 0);
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                          salt.data(), static_cast<int>(salt.size()),
                          iterations, EVP_sha256(),
                          static_cast<int>(out.size()), out.data()) != 1)
    {
        throw std::runtime_error("PKCS5_PBKDF2_HMAC failed");
    }
    return out;
}

} // namespace

PasswordHash hashPasswordWithSalt(const std::string& password,
                                  const std::vector<unsigned char>& salt,
                                  int iterations)
{
    PasswordHash result;
    result.salt = salt;
    result.hash = deriveHash(password, salt, iterations);
    result.iterations = iterations;
    result.algo = ALGO_NAME;
    return result;
}

PasswordHash hashPassword(const std::string& password)
{
    std::vector<unsigned char> salt(SALT_SIZE, 0);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1)
    {
        throw std::runtime_error("RAND_bytes failed");
    }
    return hashPasswordWithSalt(password, salt, DEFAULT_ITERATIONS);
}

bool verifyPassword(const std::string& password, const PasswordHash& stored)
{
    if (stored.algo != ALGO_NAME || stored.iterations <= 0 ||
        stored.salt.size() != SALT_SIZE || stored.hash.size() != HASH_SIZE)
    {
        return false;
    }

    std::vector<unsigned char> candidate =
        deriveHash(password, stored.salt, stored.iterations);
    return CRYPTO_memcmp(candidate.data(), stored.hash.data(), HASH_SIZE) == 0;
}

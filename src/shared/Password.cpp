#include "ecshop/shared/Password.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace ecshop::shared {

static constexpr int kPbkdf2Iterations = 100000;
static constexpr size_t kSaltBytes = 16;
static constexpr size_t kHashBytes = 32;

static std::string toHex(const unsigned char *data, size_t len)
{
    static const char *kDigits = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i)
    {
        out.push_back(kDigits[data[i] >> 4]);
        out.push_back(kDigits[data[i] & 0x0f]);
    }
    return out;
}

static std::vector<unsigned char> fromHex(const std::string &text)
{
    std::vector<unsigned char> out;
    if (text.size() % 2 != 0)
        return out;
    out.reserve(text.size() / 2);
    for (size_t i = 0; i < text.size(); i += 2)
    {
        unsigned int v = 0;
        if (std::sscanf(text.c_str() + i, "%2x", &v) != 1)
            return {};
        out.push_back(static_cast<unsigned char>(v));
    }
    return out;
}

static std::vector<unsigned char> pbkdf2(const std::string &password,
                                         const std::vector<unsigned char> &salt,
                                         int iterations, size_t out_len)
{
    std::vector<unsigned char> out(out_len);
    if (PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                          salt.data(), static_cast<int>(salt.size()), iterations,
                          EVP_sha256(), static_cast<int>(out_len), out.data()) != 1)
        throw std::runtime_error("PBKDF2 failed");
    return out;
}

std::string hashPassword(const std::string &password)
{
    std::vector<unsigned char> salt(kSaltBytes);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1)
        throw std::runtime_error("RAND_bytes failed");

    std::vector<unsigned char> hash = pbkdf2(password, salt, kPbkdf2Iterations, kHashBytes);

    char prefix[64];
    std::snprintf(prefix, sizeof(prefix), "pbkdf2_sha256$%d$", kPbkdf2Iterations);
    return std::string(prefix) + toHex(salt.data(), salt.size()) + "$" +
           toHex(hash.data(), hash.size());
}

bool verifyPassword(const std::string &password, const std::string &stored)
{
    // pbkdf2_sha256$iterations$salt_hex$hash_hex
    size_t p1 = stored.find('$');
    size_t p2 = p1 == std::string::npos ? std::string::npos : stored.find('$', p1 + 1);
    size_t p3 = p2 == std::string::npos ? std::string::npos : stored.find('$', p2 + 1);
    if (p1 == std::string::npos || p2 == std::string::npos || p3 == std::string::npos)
        return false;

    if (stored.compare(0, p1, "pbkdf2_sha256") != 0)
        return false;

    int iterations = std::atoi(stored.substr(p1 + 1, p2 - p1 - 1).c_str());
    std::vector<unsigned char> salt = fromHex(stored.substr(p2 + 1, p3 - p2 - 1));
    std::vector<unsigned char> want = fromHex(stored.substr(p3 + 1));
    if (iterations <= 0 || salt.empty() || want.empty())
        return false;

    std::vector<unsigned char> got = pbkdf2(password, salt, iterations, want.size());
    if (got.size() != want.size())
        return false;

    // constant-time compare
    unsigned char diff = 0;
    for (size_t i = 0; i < got.size(); ++i)
        diff |= got[i] ^ want[i];
    return diff == 0;
}

std::string randomTokenHex(size_t bytes)
{
    std::vector<unsigned char> buf(bytes);
    if (RAND_bytes(buf.data(), static_cast<int>(buf.size())) != 1)
        throw std::runtime_error("RAND_bytes failed");
    return toHex(buf.data(), buf.size());
}

std::string sha256Hex(const std::string &text)
{
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    if (EVP_Digest(text.c_str(), text.size(), hash, &len, EVP_sha256(), nullptr) != 1)
        throw std::runtime_error("SHA256 failed");
    return toHex(hash, len);
}

std::string hmacSha256Hex(const std::string &key, const std::string &text)
{
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    if (HMAC(EVP_sha256(), key.c_str(), static_cast<int>(key.size()),
             reinterpret_cast<const unsigned char *>(text.c_str()), text.size(), hash,
             &len) == nullptr)
        throw std::runtime_error("HMAC failed");
    return toHex(hash, len);
}

} // namespace ecshop::shared

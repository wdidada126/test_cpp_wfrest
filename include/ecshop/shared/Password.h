#pragma once

#include <string>

namespace ecshop::shared {

// PBKDF2-SHA256 password hashing (docs/03_api_contract.md).
// Stored format: "pbkdf2_sha256$<iterations>$<salt_hex>$<hash_hex>"
std::string hashPassword(const std::string &password);
bool verifyPassword(const std::string &password, const std::string &stored);

// Cryptographically random lowercase hex token, `bytes` * 2 hex chars.
std::string randomTokenHex(size_t bytes = 32);

// SHA-256 of text as lowercase hex; sessions/tokens store only this.
std::string sha256Hex(const std::string &text);

// HMAC-SHA256 of text with key, lowercase hex (payment callbacks use this)
std::string hmacSha256Hex(const std::string &key, const std::string &text);

} // namespace ecshop::shared

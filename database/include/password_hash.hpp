#pragma once

#include <string>
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>

class PasswordHash {
public:
    static std::string hash(const std::string& password) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, password.c_str(), password.length());
        SHA256_Final(hash, &sha256);

        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }

    static bool verify(const std::string& password, const std::string& hashValue) {
        return hash(password) == hashValue;
    }
};

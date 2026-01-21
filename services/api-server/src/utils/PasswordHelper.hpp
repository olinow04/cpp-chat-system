#pragma once
#include <string>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <cstring>
#include <sstream>
#include <iomanip>

/**
 * Simple bcrypt-style password hashing helper using OpenSSL
 * For production, consider using a dedicated bcrypt library
 */
class PasswordHelper {
public:
    /**
     * Hash a plaintext password
     * Returns hashed password as hex string
     */
    static std::string hashPassword(const std::string& password) {
        // Generate salt
        unsigned char salt[16];
        RAND_bytes(salt, sizeof(salt));
        
        // Hash with PBKDF2
        unsigned char hash[32];
        PKCS5_PBKDF2_HMAC(
            password.c_str(), password.length(),
            salt, sizeof(salt),
            10000,  // iterations
            EVP_sha256(),
            sizeof(hash), hash
        );
        
        // Combine salt + hash and return as hex
        std::stringstream ss;
        for(int i = 0; i < 16; i++)
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)salt[i];
        ss << ":";
        for(int i = 0; i < 32; i++)
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        
        return ss.str();
    }
    
    /**
     * Verify a plaintext password against stored hash
     */
    static bool verifyPassword(const std::string& password, const std::string& storedHash) {
        // Parse stored hash (salt: hash format)
        size_t colonPos = storedHash.find(':');
        if(colonPos == std::string::npos) return false;
        
        std::string saltHex = storedHash.substr(0, colonPos);
        std::string hashHex = storedHash.substr(colonPos + 1);
        
        // Convert hex salt back to bytes
        unsigned char salt[16];
        for(int i = 0; i < 16; i++) {
            sscanf(saltHex.substr(i*2, 2).c_str(), "%02hhx", &salt[i]);
        }
        
        // Hash the provided password with same salt
        unsigned char hash[32];
        PKCS5_PBKDF2_HMAC(
            password.c_str(), password.length(),
            salt, sizeof(salt),
            10000,
            EVP_sha256(),
            sizeof(hash), hash
        );
        
        // Convert to hex and compare
        std::stringstream ss;
        for(int i = 0; i < 32; i++)
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        
        return ss.str() == hashHex;
    }
};
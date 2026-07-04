#include "Hash.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <crypt.h>

#include "Logger.h"

namespace fileagent
{

    namespace
    {

        // bcrypt 使用的 base64 编码表（与标准 base64 不同）
        constexpr const char BCRYPT_BASE64[] =
            "./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

        /**
         * @brief bcrypt 专用的 base64 编码
         *
         * 将 input_len 字节编码为 bcrypt base64 字符串。
         * 每 3 字节 → 4 字符，剩余 1 字节 → 2 字符，剩余 2 字节 → 3 字符。
         */
        std::string encodeBcryptBase64(const std::uint8_t *input, int input_len)
        {
            std::string output;
            int i = 0;

            while (i < input_len)
            {
                uint32_t value = input[i++];
                output.push_back(BCRYPT_BASE64[value & 0x3f]);

                if (i >= input_len)
                {
                    output.push_back(BCRYPT_BASE64[(value >> 6) & 0x3f]);
                    break;
                }

                value |= (static_cast<uint32_t>(input[i++]) << 8);
                output.push_back(BCRYPT_BASE64[(value >> 6) & 0x3f]);

                if (i >= input_len)
                {
                    output.push_back(BCRYPT_BASE64[(value >> 12) & 0x3f]);
                    break;
                }

                value |= (static_cast<uint32_t>(input[i++]) << 16);
                output.push_back(BCRYPT_BASE64[(value >> 12) & 0x3f]);
                output.push_back(BCRYPT_BASE64[(value >> 18) & 0x3f]);
            }

            return output;
        }

        // 从 /dev/urandom 读取随机字节
        std::vector<std::uint8_t> getRandomBytes(int count)
        {
            std::vector<std::uint8_t> buffer(count);
            std::ifstream urandom("/dev/urandom", std::ios::binary);
            if (!urandom)
            {
                throw std::runtime_error("failed to open /dev/urandom");
            }
            urandom.read(reinterpret_cast<char *>(buffer.data()), count);
            if (urandom.gcount() != count)
            {
                throw std::runtime_error("failed to read enough random bytes from /dev/urandom");
            }
            return buffer;
        }

    } // anonymous namespace

    std::string PasswordHasher::generateToken(int byteCount)
    {
        auto random_bytes = getRandomBytes(byteCount);

        std::string token;
        token.reserve(byteCount * 2);

        constexpr const char HEX_CHARS[] = "0123456789abcdef";
        for (auto b : random_bytes)
        {
            token.push_back(HEX_CHARS[(b >> 4) & 0x0f]);
            token.push_back(HEX_CHARS[b & 0x0f]);
        }

        return token;
    }

    std::string PasswordHasher::generateSalt(int cost)
    {
        // bcrypt salt: 16 字节随机值 → 22 字符 base64
        auto random_bytes = getRandomBytes(16);
        std::string encoded = encodeBcryptBase64(random_bytes.data(), 16);

        std::string salt;
        salt.reserve(7 + 22);
        salt += "$2b$";
        if (cost < 10)
            salt += "0";
        salt += std::to_string(cost);
        salt += "$";
        salt += encoded;

        // 确保正好 29 字符（$2b$CC$ + 22 chars）
        if (salt.size() != 29)
        {
            salt.resize(29);
        }

        logDebug("Generated bcrypt salt: " + salt.substr(0, 7) + "[hidden]");
        return salt;
    }

    std::string PasswordHasher::hashPassword(const std::string &password)
    {
        if (password.empty())
        {
            logError("cannot hash empty password");
            return {};
        }

        std::string salt = generateSalt(DEFAULT_COST);

        struct crypt_data data;
        std::memset(&data, 0, sizeof(data));

        char *result = crypt_r(password.c_str(), salt.c_str(), &data);
        if (result == nullptr)
        {
            logError("bcrypt hash failed");
            return {};
        }

        logDebug("Password hashed successfully");
        return std::string(result);
    }

    bool PasswordHasher::verifyPassword(const std::string &password, const std::string &hash)
    {
        if (hash.empty() || password.empty())
        {
            return false;
        }

        // 提取 salt 前缀（前 29 字符 = $2b$CC$ + 22 base64 字符）
        std::string salt = hash.substr(0, 29);
        if (salt.size() != 29)
        {
            logError("invalid bcrypt hash format");
            return false;
        }

        struct crypt_data data;
        std::memset(&data, 0, sizeof(data));

        char *result = crypt_r(password.c_str(), salt.c_str(), &data);
        if (result == nullptr)
        {
            logError("bcrypt verify failed");
            return false;
        }

        bool matched = (hash == result);
        logDebug(matched ? "Password verified successfully" : "Password verification failed");
        return matched;
    }

} // namespace fileagent

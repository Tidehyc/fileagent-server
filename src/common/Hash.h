#pragma once

#include <string>

namespace fileagent
{

    /**
     * @brief bcrypt 密码哈希工具类
     *
     * 使用系统 libcrypt 的 crypt_r() 实现 bcrypt 密码哈希。
     * 线程安全（crypt_r 使用线程本地数据）。
     */
    class PasswordHasher
    {
    public:
        /**
         * @brief 对密码进行 bcrypt 哈希
         * @param password 明文密码
         * @return bcrypt 哈希字符串（格式: $2b$<cost>$<salt><hash>）
         */
        static std::string hashPassword(const std::string &password);

        /**
         * @brief 验证密码与哈希是否匹配
         * @param password 明文密码
         * @param hash     bcrypt 哈希字符串
         * @return true 匹配, false 不匹配
         */
        static bool verifyPassword(const std::string &password, const std::string &hash);

        /**
         * @brief 生成安全随机令牌（hex 编码）
         * @param byteCount 随机字节数（默认 32 → 64 hex 字符）
         * @return hex 编码的令牌字符串
         */
        static std::string generateToken(int byteCount = 32);

    private:
        /**
         * @brief 生成 bcrypt salt 字符串
         * @param cost 哈希强度（推荐 10-12）
         * @return $2b$<cost>$<22字符base64salt>
         */
        static std::string generateSalt(int cost = 10);

        static constexpr int DEFAULT_COST = 10;
        static constexpr int SALT_LENGTH = 22;
    };

} // namespace fileagent

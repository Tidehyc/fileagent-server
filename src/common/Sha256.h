#pragma once

#include <string>
#include <vector>

namespace fileagent
{

    /**
     * @brief 使用 OpenSSL EVP 计算 SHA-256 哈希
     * @param data 输入数据
     * @return 哈希字节数组
     */
    std::vector<unsigned char> sha256(const std::string &data);

    /**
     * @brief 将字节数组转为十六进制字符串
     * @param data 二进制数据
     * @return 小写 hex 字符串
     */
    std::string toHexString(const std::vector<unsigned char> &data);

} // namespace fileagent

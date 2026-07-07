#include "Sha256.h"

#include <openssl/evp.h>

namespace fileagent
{

    std::vector<unsigned char> sha256(const std::string &data)
    {
        std::vector<unsigned char> hash(EVP_MAX_MD_SIZE);
        unsigned int hash_len = 0;

        EVP_MD_CTX *ctx = EVP_MD_CTX_new();
        if (!ctx) return {};

        EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
        EVP_DigestUpdate(ctx, data.data(), data.size());
        EVP_DigestFinal_ex(ctx, hash.data(), &hash_len);
        EVP_MD_CTX_free(ctx);

        hash.resize(hash_len);
        return hash;
    }

    std::string toHexString(const std::vector<unsigned char> &data)
    {
        static const char hex_chars[] = "0123456789abcdef";
        std::string result;
        result.reserve(data.size() * 2);
        for (unsigned char c : data)
        {
            result += hex_chars[c >> 4];
            result += hex_chars[c & 0x0F];
        }
        return result;
    }

} // namespace fileagent

#include "byte_tools.h"
#include <openssl/sha.h>
#include <vector>
#include <netinet/in.h>
#include <cstring>

int BytesToInt(std::string_view bytes) {
    uint32_t bytes_val;
    std::memcpy(&bytes_val, bytes.data(), 4);
    uint32_t int_val = ntohl(bytes_val);
    return static_cast<int>(int_val);
}

std::string IntToBytes(uint32_t chsl) {
    uint32_t net_val = htonl(chsl);
    std::string future_int(4, '\0');
    std::memcpy(&future_int[0], &net_val, 4);
    return future_int;
}

std::string CalculateSHA1(const std::string& msg) {
    std::vector<uint8_t> hash(SHA_DIGEST_LENGTH);
    SHA1(reinterpret_cast<const unsigned char *>(msg.data()), msg.size(), hash.data());
    return std::string(reinterpret_cast<char *>(hash.data()), hash.size());
}


std::string HexEncode(const std::string &input) {
    static const char kHexDigits[] = "0123456789abcdef";

    std::string encoded;
    encoded.reserve(input.size() * 2);
    for (unsigned char byte : input) {
        encoded.push_back(kHexDigits[byte >> 4]);
        encoded.push_back(kHexDigits[byte & 0x0F]);
    }

    return encoded;
}

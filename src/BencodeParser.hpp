#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <variant>
#include <list>
#include <map>
#include <sstream>
#include <openssl/sha.h>

namespace Bencode {
    struct BencodeTypes;
    using BencodeList = std::vector<BencodeTypes>;
    using BencodeDict = std::map<std::string, BencodeTypes>;

    struct BencodeTypes {
        std::variant<int, std::string, BencodeList, BencodeDict> value;

        template<typename T>
        T &get(){
            return std::get<T>(value);
        }

        template<typename T>
        const T &get() const{
            return std::get<T>(value);
        }
    };

    class BencodeParser {
    public:
        BencodeParser(const std::string &data_);

        BencodeTypes ParseAny();
        std::string ComputeSHA1();

    private:
        BencodeTypes ParseInt(char stop_sign = 'e');
        BencodeTypes ParseString();
        BencodeTypes ParseList();
        BencodeTypes ParseDict();
        bool IsInt();

        std::string data;
        int i = 0, start_for_hashin = 0, end_for_hashin = 0;
    };
}


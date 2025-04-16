#include <BencodeParser.hpp>

template<typename T>
T& BencodeTypes::get() {
    return std::get<T>(value);
}

template<typename T>
const T& BencodeTypes::get() const{
    return std::get<T>(value);
}

BencodeTypes BencodeParser::ParseAny() {
    char t = data[i];
    if (IsInt()) t = '0';
    switch (t) {
        case 'i':
            return ParseInt();
        case '0':
            return ParseString();
        case 'l':
            return ParseList();
        case 'd':
            return ParseDict();
        default:
            return BencodeTypes{0};
    }
}

std::string BencodeParser::ComputeSHA1() {
    std::string to_hash = data.substr(start_for_hashin, end_for_hashin - start_for_hashin);
    std::vector <uint8_t> hash(SHA_DIGEST_LENGTH);
    SHA1(reinterpret_cast<const unsigned char *>(to_hash.data()), to_hash.size(), hash.data());
    return std::string(reinterpret_cast<char *>(hash.data()), hash.size());
}

BencodeTypes BencodeParser::ParseInt(char stop_sign) {
    ++i;
    int ans = 0;
    bool negative = false;
    if (data[i] == '-') {
        negative = true;
        ++i;
    }
    while (data[i] != stop_sign) {
        ans = (ans * 10) + (data[i] - '0');
        ++i;
    }
    ++i;
    return BencodeTypes{(-1 * negative) * ans};
}

BencodeTypes BencodeParser::ParseString() {
    int size = 0;
    while (IsInt()) {
        size = size * 10 + (data[i] - '0');
        ++i;
    }
    ++i;
    std::string ans;
    ans.reserve(size);
    for (int j = 0; j < size; ++j) {
        ans.push_back(data[i]);
        ++i;
    }
    if (ans == "info") start_for_hashin = i;
    return BencodeTypes{ans};
}

BencodeTypes BencodeParser::ParseList() {
    ++i;
    BencodeList ans;
    while (data[i] != 'e') ans.push_back(ParseAny());
    ++i;
    return BencodeTypes{ans};
}

BencodeTypes BencodeParser::ParseDict() {
    ++i;
    BencodeDict dict;
    while (data[i] != 'e') {
        std::string key = ParseString().get<std::string>();
        BencodeTypes value = ParseAny();
        dict[key] = value;
        if (key == "info") end_for_hashin = i;
    }
    ++i;
    return BencodeTypes{dict};
}
#include "torrent_file.h"
#include "bencode.h"
#include <vector>
#include <openssl/sha.h>
#include <fstream>
#include <variant>
#include <sstream>

TorrentFile LoadTorrentFile(const std::string &filename) {
    struct TorrentFile tor;

    std::ifstream file(filename, std::ios::binary);
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string files_data = ss.str();

    Bencode::Bedecoder decoder(files_data);
    Bencode::BencodeDict data = decoder.ParseAny().get<Bencode::BencodeDict>();
    if (data.find("announce") != data.end()) tor.announce = data["announce"].get<std::string>();
    if (data.find("comment") != data.end()) tor.comment = data["comment"].get<std::string>();
    if (data.find("info") != data.end()) {
        auto infa = data["info"].get<Bencode::BencodeDict>();
        if (infa.find("name") != infa.end()) tor.name = infa["name"].get<std::string>();
        if (infa.find("length") != infa.end()) tor.length = static_cast<size_t>(infa["length"].get<int64_t>());
        if (infa.find("piece length") != infa.end()) tor.pieceLength = static_cast<size_t>(infa["piece length"].get<int64_t>());
        if (infa.find("pieces") != infa.end()) {
            std::string pieces = infa["pieces"].get<std::string>();
            for (size_t i = 0; i < pieces.size(); i += SHA_DIGEST_LENGTH) {
                if (i + SHA_DIGEST_LENGTH <= pieces.size()) {
                    tor.pieceHashes.emplace_back(pieces.begin() + i, pieces.begin() + i + SHA_DIGEST_LENGTH);
                }
            }
        }
    }
    tor.infoHash = decoder.ComputeSHA1();
    return tor;
}

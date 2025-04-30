#include "TorrentTracker.hpp"
#include "BencodeParser.hpp"
#include <cpr/cpr.h>
#include <iostream>

const std::vector<Peer> &TorrentTracker::GetPeers() const {
    return peers_;
}

TorrentTracker::TorrentTracker(const std::string &url) : url_(url) {}

void TorrentTracker::UpdatePeers(const TorrentFile& tf, std::string peerId, int port){
    cpr::Response res = cpr::Get(
            cpr::Url{tf.announce},
            cpr::Parameters {
                    {"info_hash", tf.infoHash},
                    {"peer_id", peerId},
                    {"port", std::to_string(port)},
                    {"uploaded", std::to_string(0)},
                    {"downloaded", std::to_string(0)},
                    {"left", std::to_string(tf.length)},
                    {"compact", std::to_string(1)}
            },
            cpr::Timeout{20000}
    );
    peers_.clear();
    Bencode::BencodeParser decoder(res.text);
    auto answer = decoder.ParseAny().get<Bencode::BencodeDict>();
    std::string coded_peers = answer["peers"].get<std::string>();
    for(size_t i = 0; i < coded_peers.size(); i += 6){
        Peer pir;
        for(size_t j = i; j < i + 4; ++j) pir.ip += std::to_string(static_cast<uint8_t>(coded_peers[j])) + '.';
        pir.ip.pop_back();
        pir.port = (static_cast<uint8_t>(coded_peers[i + 4]) << 8) + static_cast<uint8_t>(coded_peers[i + 5]);
        peers_.push_back(pir);
    }
}


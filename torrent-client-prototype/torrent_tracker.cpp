#include "torrent_tracker.h"
#include "bencode.h"
#include "byte_tools.h"
#include <cpr/cpr.h>
#include <iostream>

const std::vector<Peer> &TorrentTracker::GetPeers() const {
    return peers_;
}

TorrentTracker::TorrentTracker(const std::string &url) : url_(url) {}

void TorrentTracker::UpdatePeers(const TorrentFile& tf, std::string peerId, int port){
    cpr::Response res = cpr::Get(
            cpr::Url{url_},
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

    if (res.status_code != 200) {
        std::cerr << "Tracker request failed with status code " << res.status_code << std::endl;
        return;
    }
    if (res.text.empty()) {
        std::cerr << "Tracker returned empty response" << std::endl;
        return;
    }

    std::string coded_peers;
    try {
        Bencode::Bedecoder decoder(res.text);
        Bencode::BencodeTypes parsed = decoder.ParseAny();
        const auto* answer = std::get_if<Bencode::BencodeDict>(&parsed.value);
        if (!answer) {
            std::cerr << "Tracker returned non-dictionary response" << std::endl;
            return;
        }

        const auto failure = answer->find("failure reason");
        if (failure != answer->end()) {
            if (const auto* reason = std::get_if<std::string>(&failure->second.value)) {
                std::cerr << "Tracker failure: " << *reason << std::endl;
            } else {
                std::cerr << "Tracker returned failure response" << std::endl;
            }
            return;
        }

        const auto peers_it = answer->find("peers");
        if (peers_it == answer->end()) {
            std::cerr << "Tracker response does not contain peers" << std::endl;
            return;
        }
        const auto* peers_string = std::get_if<std::string>(&peers_it->second.value);
        if (!peers_string) {
            std::cerr << "Tracker peers field is not a compact string" << std::endl;
            return;
        }
        coded_peers = *peers_string;
    } catch (const std::exception& e) {
        std::cerr << "Cannot parse tracker response: " << e.what() << std::endl;
        return;
    }

    for(size_t i = 0; i < coded_peers.size(); i += 6){
        if (i + 6 > coded_peers.size()) {
            break;
        }
        Peer pir;
        for(size_t j = i; j < i + 4; ++j) pir.ip += std::to_string(static_cast<uint8_t>(static_cast<unsigned char>(coded_peers[j]))) + '.';
        pir.ip.pop_back();
        pir.port = (static_cast<uint8_t>(static_cast<unsigned char>(coded_peers[i + 4])) << 8) +
                   static_cast<uint8_t>(static_cast<unsigned char>(coded_peers[i + 5]));
        peers_.push_back(pir);
    }
}


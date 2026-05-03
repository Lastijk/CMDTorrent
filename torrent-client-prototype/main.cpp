#include "torrent_tracker.h"
#include "piece_storage.h"
#include "peer_connect.h"
#include <iostream>
#include <filesystem>
#include <random>
#include <thread>
#include <algorithm>
#include <stdexcept>
#include <cstdlib>

namespace fs = std::filesystem;

std::mutex cerrMutex, coutMutex;

std::string RandomString(size_t length) {
    std::random_device random;
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result.push_back(random() % ('Z' - 'A') + 'A');
    }
    return result;
}

const std::string PeerId = "TESTAPPDONTWORRY" + RandomString(4);

bool RunDownloadMultithread(PieceStorage& pieces, const TorrentFile& torrentFile, const std::string& ourId,
                              const TorrentTracker& tracker, size_t piecesToDownload) {
    using namespace std::chrono_literals;

    std::vector<std::thread> peerThreads;
    std::vector<std::shared_ptr<PeerConnect>> peerConnections;

    for (const Peer& peer : tracker.GetPeers()) {
        peerConnections.emplace_back(std::make_shared<PeerConnect>(peer, torrentFile, ourId, pieces));
    }

    peerThreads.reserve(peerConnections.size());

    for (auto& peerConnectPtr : peerConnections) {
        peerThreads.emplace_back([peerConnectPtr]() {
            bool tryAgain = true;
            int attempts = 0;
            do {
                try {
                    ++attempts;
                    peerConnectPtr->Run();
                } catch (const std::runtime_error& e) {
                    std::lock_guard<std::mutex> cerrLock(cerrMutex);
                    std::cerr << "Runtime error: " << e.what() << std::endl;
                } catch (const std::exception& e) {
                    std::lock_guard<std::mutex> cerrLock(cerrMutex);
                    std::cerr << "Exception: " << e.what() << std::endl;
                } catch (...) {
                    std::lock_guard<std::mutex> cerrLock(cerrMutex);
                    std::cerr << "Unknown error" << std::endl;
                }
                tryAgain = peerConnectPtr->Failed() && attempts < 3;
            } while (tryAgain);
        });
    }

    {
        std::lock_guard<std::mutex> coutLock(coutMutex);
        std::cout << "Started " << peerThreads.size() << " threads for peers" << std::endl;
    }

    std::this_thread::sleep_for(10s);
    while (pieces.PiecesSavedToDiscCount() < piecesToDownload) {
        if (pieces.PiecesInProgressCount() == 0) {
            {
                std::lock_guard<std::mutex> coutLock(coutMutex);
                std::cout << "Want to download more pieces but all peer connections are not working. Let's request "
                             "new peers"
                          << std::endl;
            }

            for (auto& peerConnectPtr : peerConnections) {
                peerConnectPtr->Terminate();
            }
            for (std::thread& thread : peerThreads) {
                thread.join();
            }
            return true;
        }
        std::this_thread::sleep_for(1s);
    }

    {
        std::lock_guard<std::mutex> coutLock(coutMutex);
        std::cout << "Terminating all peer connections" << std::endl;
    }
    for (auto& peerConnectPtr : peerConnections) {
        peerConnectPtr->Terminate();
    }

    for (std::thread& thread : peerThreads) {
        thread.join();
    }

    return false;
}

void DownloadTorrentFile(const TorrentFile& torrentFile, PieceStorage& pieces, const std::string& ourId,
                         size_t piecesToDownload) {
    std::cout << "Connecting to tracker " << torrentFile.announce << std::endl;
    TorrentTracker tracker(torrentFile.announce);
    bool requestMorePeers = false;
    do {
        tracker.UpdatePeers(torrentFile, ourId, 12345);

        if (tracker.GetPeers().empty()) {
            std::cerr << "No peers found. Cannot download a file" << std::endl;
        }

        std::cout << "Found " << tracker.GetPeers().size() << " peers" << std::endl;
        for (const Peer& peer : tracker.GetPeers()) {
            std::cout << "Found peer " << peer.ip << ":" << peer.port << std::endl;
        }

        requestMorePeers = RunDownloadMultithread(pieces, torrentFile, ourId, tracker, piecesToDownload);
    } while (requestMorePeers);
}

void PrintUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " -d <download_directory> -p <percent> <path_to.torrent>\n"
              << "  -d  каталог, куда сохраняется файл из торрента\n"
              << "  -p  сколько процентов частей скачать с начала (целое, как в задании: N*total/100 частей)\n";
}

int main(int argc, char* argv[]) {
    std::string downloadDir;
    int percent = -1;
    std::string torrentPath;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-d") {
            if (i + 1 >= argc) {
                PrintUsage(argv[0]);
                return 1;
            }
            downloadDir = argv[++i];
        } else if (arg == "-p") {
            if (i + 1 >= argc) {
                PrintUsage(argv[0]);
                return 1;
            }
            try {
                percent = std::stoi(argv[++i]);
            } catch (...) {
                std::cerr << "Invalid -p value\n";
                return 1;
            }
            if (percent < 0 || percent > 100) {
                std::cerr << "-p must be between 0 and 100\n";
                return 1;
            }
        } else if (arg == "-h" || arg == "--help") {
            PrintUsage(argv[0]);
            return 0;
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << '\n';
            PrintUsage(argv[0]);
            return 1;
        } else {
            torrentPath = arg;
        }
    }

    if (downloadDir.empty() || percent < 0 || torrentPath.empty()) {
        PrintUsage(argv[0]);
        return 1;
    }

    TorrentFile torrentFile;
    try {
        torrentFile = LoadTorrentFile(torrentPath);
        std::cout << "Loaded torrent file " << torrentPath << ". " << torrentFile.comment << std::endl;
    } catch (const std::invalid_argument& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    const size_t totalPieces = torrentFile.pieceHashes.size();
    const size_t piecesToDownload = totalPieces * static_cast<size_t>(percent) / 100;

    if (piecesToDownload == 0) {
        std::cerr << "Nothing to download: " << percent << "% of " << totalPieces << " pieces rounds down to 0\n";
        return 1;
    }

    std::cout << "Will download first " << piecesToDownload << " of " << totalPieces << " pieces (" << percent
              << "%)\n";

    const fs::path outputDirectory = fs::absolute(downloadDir);
    PieceStorage pieces(torrentFile, outputDirectory, piecesToDownload);

    DownloadTorrentFile(torrentFile, pieces, PeerId, piecesToDownload);

    if (pieces.PiecesSavedToDiscCount() < piecesToDownload) {
        std::cerr << "Download incomplete: got " << pieces.PiecesSavedToDiscCount() << " / " << piecesToDownload
                  << " pieces\n";
        return 1;
    }

    pieces.CloseOutputFile();
    std::cout << "Saved to " << (outputDirectory / torrentFile.name) << std::endl;
    return 0;
}

#include "piece_storage.h"
#include <stdexcept>
#include <iostream>
#include <algorithm>

namespace {
std::mutex coutMutex;
}

PieceStorage::PieceStorage(const TorrentFile& tf, const std::filesystem::path& outputDirectory, size_t piecesToDownload)
    : outputFilePath_(outputDirectory / tf.name),
      pieceLength_(tf.pieceLength),
      totalPiecesCount_(tf.pieceHashes.size()),
      piecesToDownload_(std::min(piecesToDownload, tf.pieceHashes.size())) {
    std::filesystem::create_directories(outputFilePath_.parent_path());

    {
        std::ofstream createFile(outputFilePath_, std::ios::binary | std::ios::trunc);
        if (!createFile.is_open()) {
            throw std::runtime_error("Cannot create output file: " + outputFilePath_.string());
        }
    }
    std::filesystem::resize_file(outputFilePath_, tf.length);

    outputFile_.open(outputFilePath_, std::ios::binary | std::ios::in | std::ios::out);
    if (!outputFile_.is_open()) {
        throw std::runtime_error("Cannot open output file: " + outputFilePath_.string());
    }

    for (size_t index = 0; index < piecesToDownload_; ++index) {
        size_t pieceSize = tf.pieceLength;
        if (index + 1 == tf.pieceHashes.size()) {
            const size_t lastPieceOffset = index * tf.pieceLength;
            pieceSize = tf.length > lastPieceOffset ? tf.length - lastPieceOffset : 0;
        }
        PiecePtr piece = std::make_shared<Piece>(index, pieceSize, tf.pieceHashes[index]);
        allPieces_.push_back(piece);
        remainPieces_.push(piece);
    }
}

PiecePtr PieceStorage::GetNextPieceToDownload() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (remainPieces_.empty() && queueClosed_ && piecesInProgress_.empty()) {
        RefillQueue();
    }
    if (remainPieces_.empty()) {
        return nullptr;
    }

    PiecePtr piece = remainPieces_.front();
    remainPieces_.pop();
    piecesInProgress_.insert(piece->GetIndex());
    return piece;
}

void PieceStorage::PieceProcessed(const PiecePtr& piece) {
    if (!piece) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    piecesInProgress_.erase(piece->GetIndex());

    if (!piece->HashMatches()) {
        piece->Reset();
        if (!queueClosed_) {
            remainPieces_.push(piece);
        }
        return;
    }

    if (piecesSavedToDisc_.find(piece->GetIndex()) == piecesSavedToDisc_.end()) {
        SavePieceToDisk(piece);
        piecesSavedToDisc_.insert(piece->GetIndex());
        piecesSavedToDiscIndices_.push_back(piece->GetIndex());
        std::lock_guard<std::mutex> coutLock(coutMutex);
        std::cout << "Saved piece " << piece->GetIndex() << " to disk ("
                  << piecesSavedToDiscIndices_.size() << "/" << totalPiecesCount_ << ")" << std::endl;
    }

    while (!remainPieces_.empty()) {
        remainPieces_.pop();
    }
    queueClosed_ = true;
}

bool PieceStorage::QueueIsEmpty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return remainPieces_.empty();
}

size_t PieceStorage::PiecesSavedToDiscCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return piecesSavedToDiscIndices_.size();
}

size_t PieceStorage::TotalPiecesCount() const {
    return piecesToDownload_;
}

void PieceStorage::CloseOutputFile() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (outputFile_.is_open()) {
        outputFile_.flush();
        outputFile_.close();
    }
}

const std::vector<size_t>& PieceStorage::GetPiecesSavedToDiscIndices() const {
    return piecesSavedToDiscIndices_;
}

size_t PieceStorage::PiecesInProgressCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return piecesInProgress_.size();
}

void PieceStorage::SavePieceToDisk(const PiecePtr& piece) {
    if (!outputFile_.is_open()) {
        throw std::runtime_error("Output file is not open");
    }

    outputFile_.clear();
    outputFile_.seekp(static_cast<std::streamoff>(piece->GetIndex() * pieceLength_), std::ios::beg);
    if (!outputFile_.good()) {
        throw std::runtime_error("Cannot seek output file");
    }

    const std::string data = piece->GetData();
    outputFile_.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!outputFile_.good()) {
        throw std::runtime_error("Cannot write piece to output file");
    }
    outputFile_.flush();
}

void PieceStorage::RefillQueue() {
    queueClosed_ = false;
    for (const PiecePtr& piece : allPieces_) {
        const size_t index = piece->GetIndex();
        if (piecesSavedToDisc_.find(index) == piecesSavedToDisc_.end()
            && piecesInProgress_.find(index) == piecesInProgress_.end()) {
            piece->Reset();
            remainPieces_.push(piece);
        }
    }
}

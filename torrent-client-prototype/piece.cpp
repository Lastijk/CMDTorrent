#include "byte_tools.h"
#include "piece.h"
#include <iostream>
#include <algorithm>

namespace {
    constexpr size_t BLOCK_SIZE = 1 << 14;
}

Piece::Piece(size_t index, size_t length, std::string hash) : index_(index), length_(length), hash_(hash){
    size_t block_size = (length_ + BLOCK_SIZE - 1) / BLOCK_SIZE;
    blocks_.resize(block_size);
    for (size_t i = 0; i < block_size; ++i) {
        size_t len = BLOCK_SIZE;
        if (i == block_size - 1 && length_ % BLOCK_SIZE) {
            len = length_ - i * BLOCK_SIZE;
        }
        Block block;
        block.piece = index;
        block.offset = i * BLOCK_SIZE;
        block.length = len;
        block.status = Block::Status::Missing;
        blocks_[i] = block;
    }
}

Block *Piece::FirstMissingBlock() {
    for(auto& block : blocks_){
        if(block.status == Block::Status::Missing) return &block;
    }
    return nullptr;
}

void Piece::SaveBlock(size_t blockOffset, std::string data) {
    if(blocks_[blockOffset / BLOCK_SIZE].status == Block::Status::Pending){
        blocks_[blockOffset / BLOCK_SIZE].data = std::move(data);
        blocks_[blockOffset / BLOCK_SIZE].status = Block::Status::Retrieved;
    }
}

bool Piece::AllBlocksRetrieved() const {
    for(const auto& block : blocks_){
        if(block.status != Block::Status::Retrieved) return false;
    }
    return true;
}

size_t Piece::GetIndex() const {
    return index_;
}

void Piece::Reset() {
    for(auto& block : blocks_){
        block.data.clear();
        block.status = Block::Status::Missing;
    }
}

std::string Piece::GetData() const {
    std::string data;
    for(auto& block : blocks_) data += block.data;
    return data;
}

const std::string &Piece::GetHash() const {
    return hash_;
}

std::string Piece::GetDataHash() const {
    return CalculateSHA1(GetData());
}

bool Piece::HashMatches() const {
    return hash_ == GetDataHash();
}
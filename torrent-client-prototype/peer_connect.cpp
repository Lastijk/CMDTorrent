#include "byte_tools.h"
#include "peer_connect.h"
#include "message.h"
#include <iostream>
#include <sstream>
#include <utility>
#include <cassert>


PeerConnect::PeerConnect(const Peer &peer, const TorrentFile &tf, std::string selfPeerId, PieceStorage& pieceStorage)
    : tf_(tf),
      socket_(peer),
      selfPeerId_(std::move(selfPeerId)),
      terminated_(false),
      choked_(true),
      pieceStorage_(pieceStorage),
      pendingBlock_(false),
      failed_(false) {

}

PeerConnect::~PeerConnect() {
    socket_.CloseConnection();
}

void PeerConnect::Run() {
    if (terminated_) {
        return;
    }

    choked_ = true;
    pendingBlock_ = false;
    pieceInProgress_ = nullptr;
    piecesAvailability_ = PeerPiecesAvailability();

    RequestPiece();
    if (!pieceInProgress_) {
        return;
    }

    if (EstablishConnection()) {
        MainLoop();
    } else {
        pieceStorage_.PieceProcessed(pieceInProgress_);
        pieceInProgress_ = nullptr;
        if (!terminated_) {
            std::cerr << "Cannot establish connection to peer" << std::endl;
        }
    }
    socket_.CloseConnection();
}

void PeerConnect::PerformHandshake() {
    if (terminated_) return;
    std::string salam_msg;
    salam_msg.push_back(static_cast<char>(19));
    salam_msg += "BitTorrent protocol";
    salam_msg.insert(salam_msg.end(), 8, 0);
    salam_msg += tf_.infoHash;
    salam_msg += selfPeerId_;
    socket_.SendData(salam_msg);
    std::string servers_answer = socket_.ReceiveData(68);
    try {
        if (servers_answer[0] != 19 || servers_answer.size() != 68 ||
            servers_answer.substr(1, 19) != "BitTorrent protocol" || servers_answer.substr(28, 20) != tf_.infoHash) {
            throw std::runtime_error("Invalid handshake");
        }
        peerId_ = servers_answer.substr(48, 20);
    }
    catch (...) {
        throw std::runtime_error("Bad bitfield string access, length is: " + std::to_string(servers_answer.size()));
    }
}

bool PeerConnect::EstablishConnection() {
    failed_ = false;
    try {
        try {
            socket_.EstablishConnection();
        }
        catch (std::exception &e) {
            failed_ = !terminated_;
            return false;
        }
        PerformHandshake();
        ReceiveBitfield();
        SendInterested();
        return true;
    } catch (const std::exception &e) {
        failed_ = !terminated_;
        if (!terminated_) {
            std::cerr << "Failed to establish connection with peer " << socket_.GetIp() << ":" <<
                      socket_.GetPort() << " -- " << e.what() << std::endl;
        }
        return false;
    }
}

void PeerConnect::ReceiveBitfield() {
    if (terminated_) return;
    std::string servers_answer = socket_.ReceiveData();
    Message servers_msg = Message::Parse(servers_answer);
    if (servers_msg.id == MessageId::Unchoke) {
        choked_ = false;
    } else if (servers_msg.id == MessageId::BitField) {
        PeerPiecesAvailability pa(servers_msg);
        piecesAvailability_ = pa;
    } else if (servers_msg.id == MessageId::KeepAlive) {
        ReceiveBitfield();
    } else {
        throw std::runtime_error("Tried to receive a Bitfield message, but got wrong type");
    }
}

void PeerConnect::SendInterested() {
    if (terminated_) return;
    Message interesting{MessageId::Interested, 1, ""};
    socket_.SendData(interesting.ToString());
}

void PeerConnect::Terminate() {
    if (terminated_.exchange(true)) {
        return;
    }
    socket_.CloseConnection();
}

void PeerConnect::RequestPiece() {
    std::string pd;
    if (pieceInProgress_ && !piecesAvailability_.IsPieceAvailable(pieceInProgress_->GetIndex())) {
        pieceStorage_.PieceProcessed(pieceInProgress_);
        pieceInProgress_ = nullptr;
        pendingBlock_ = false;
    }
    size_t piecesChecked = 0;
    while(!pieceInProgress_ && piecesChecked < pieceStorage_.TotalPiecesCount()){
        pieceInProgress_ = pieceStorage_.GetNextPieceToDownload();
        if(!pieceInProgress_) return;
        ++piecesChecked;
        if (!piecesAvailability_.IsPieceAvailable(pieceInProgress_->GetIndex())) {
            pieceStorage_.PieceProcessed(pieceInProgress_);
            pieceInProgress_ = nullptr;
        }
    }
    if (!pieceInProgress_) {
        return;
    }
    if (choked_) {
        return;
    }
    Block* requested_block = pieceInProgress_->FirstMissingBlock();
    if(!requested_block){
        pieceStorage_.PieceProcessed(pieceInProgress_);
        pendingBlock_ = false;
        pieceInProgress_ = nullptr;
        return ;
    }
    std::string request_payload;
    request_payload += IntToBytes((uint32_t)requested_block->piece);
    request_payload += IntToBytes(requested_block->offset);
    request_payload += IntToBytes(requested_block->length);
    Message request_message = Message::Init(MessageId::Request, request_payload);
    socket_.SendData(request_message.ToString());
    pendingBlock_ = true;
    requested_block->status = Block::Status::Pending;
}

void PeerConnect::MainLoop() {
    while(!terminated_){
        Message servers_msg;
        try{
            if(!pendingBlock_) RequestPiece();
            servers_msg = Message::Parse(socket_.ReceiveData());
        }
        catch(std::exception& e){
            failed_ = !terminated_;
            if (pieceInProgress_) {
                pieceStorage_.PieceProcessed(pieceInProgress_);
                pieceInProgress_ = nullptr;
                pendingBlock_ = false;
            }
            return ;
        }
        switch (servers_msg.id) {
            case MessageId::Choke:
                choked_ = true;
                if (pieceInProgress_) {
                    pieceStorage_.PieceProcessed(pieceInProgress_);
                    pieceInProgress_ = nullptr;
                    pendingBlock_ = false;
                }
                break;
            case MessageId::Unchoke:
                choked_ = false;
                break;
            case MessageId::Have:
                piecesAvailability_.SetPieceAvailability(BytesToInt(servers_msg.payload));
                if (pieceInProgress_ && !piecesAvailability_.IsPieceAvailable(pieceInProgress_->GetIndex())) {
                    pieceInProgress_ = nullptr;
                    pendingBlock_ = false;
                }
                break;
            case MessageId::Piece: {
                if (!pieceInProgress_) break;
                std::string piece_data(servers_msg.payload.substr(8));
                int index = BytesToInt(servers_msg.payload);
                int offset = BytesToInt(servers_msg.payload.substr(4));
                if (static_cast<size_t>(index) == pieceInProgress_->GetIndex()) {
                    pieceInProgress_->SaveBlock(offset, piece_data);
                    pendingBlock_ = false;
                }
                break;
            }
            default:
                break;
        }
    }
}

PeerPiecesAvailability::PeerPiecesAvailability() = default;

PeerPiecesAvailability::PeerPiecesAvailability(std::string bitfield) : bitfield_(bitfield){}

PeerPiecesAvailability::PeerPiecesAvailability(const Message& msg) : bitfield_(msg.payload){}

bool PeerPiecesAvailability::IsPieceAvailable(size_t pieceIndex) const{
    size_t i = pieceIndex / 8;
    if (i >= bitfield_.size()) {
        return bitfield_.empty();
    }
    size_t j = 7 - pieceIndex % 8;
    return (static_cast<unsigned char>(bitfield_[i]) >> j) % 2;
}

void PeerPiecesAvailability::SetPieceAvailability(size_t pieceIndex){
    size_t i = pieceIndex / 8;
    if (i >= bitfield_.size()) return;
    size_t j = 7 - pieceIndex % 8;
    bitfield_[i] |= (1 << j);
}

size_t PeerPiecesAvailability::Size() const{
    return bitfield_.size();
}

bool PeerConnect::Failed() const {
    return failed_;
}

#include "TCP_Conncet.hpp"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <chrono>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>

TcpConnect::TcpConnect(std::string ip, int port, std::chrono::milliseconds connectTimeout,
                       std::chrono::milliseconds readTimeout) : ip_(ip), port_(port), connectTimeout_(connectTimeout),
                                                                readTimeout_(readTimeout) {}

TcpConnect::~TcpConnect() {
    CloseConnection();
}

void TcpConnect::EstablishConnection() {
    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    ErrorCheck(sock_, "Couldn`t make a socket");
    int f_values = fcntl(sock_, F_GETFL, 0);
    ErrorCheck(fcntl(sock_, F_SETFL, f_values | O_NONBLOCK), "Couldn`t set the mode to unblocked");
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port_);
    int convert_res = inet_pton(AF_INET, ip_.c_str(), &server.sin_addr);
    ErrorCheck(convert_res, "No valid address family");
    if (convert_res == 0) {
        CloseConnection();
        throw std::runtime_error("IP doesn`t contain a valid network address");
    }
    int connection_res = connect(sock_, (const sockaddr *) &server, sizeof(server));
    if (connection_res == -1 && errno != EINPROGRESS) ErrorCheck(convert_res, "Couldn`t establish connection");
    timeval timeout_time{};
    timeout_time.tv_usec = (connectTimeout_.count() % 1000) * 1000;
    timeout_time.tv_sec = connectTimeout_.count() / 1000;
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sock_, &fdset);
    int connect_res = select(sock_ + 1, NULL, &fdset, NULL, &timeout_time);
    switch (connect_res) {
        case -1:
            CloseConnection();
            throw std::system_error(errno, std::system_category(), "Couldn`t get a proper connection");
        case 0:
            throw std::runtime_error("Timeout on connection");
        default:
            int so_error;
            socklen_t len = sizeof(so_error);
            getsockopt(sock_, SOL_SOCKET, SO_ERROR, &so_error, &len);
            if (so_error != 0) {
                CloseConnection();
                throw std::system_error(so_error, std::system_category(), "Failed connection");
            }
    }
}

void TcpConnect::SendData(const std::string &data) const {
    ssize_t bytes_sent = send(sock_, data.c_str(), data.size(), 0);
    ErrorCheck(bytes_sent, "Couldn't send the data");
    if (static_cast<size_t>(bytes_sent) != data.size()) {
//        CloseConnection();
        throw std::runtime_error("Not all data was sent");
    }
}

std::string TcpConnect::ReceiveData(size_t bufferSize) const {
    pollfd pdf{};
    pdf.fd = sock_;
    pdf.events = POLLIN;
    if (bufferSize == 0) {
        char sizeBytes[4];
        int pollres = poll(&pdf, 1, readTimeout_.count());
        ErrorCheck(pollres, "Polling Error on size receive");
        if (pollres == 0) {
            CloseConnection();
            throw std::system_error(errno, std::system_category(), "Timeout on reading size");
        }
        ssize_t bytes_read = recv(sock_, sizeBytes, sizeof(sizeBytes), 0);
        ErrorCheck(bytes_read, "Couldn't receive size data");
        if (bytes_read != sizeof(sizeBytes)) {
            CloseConnection();
            throw std::runtime_error("Size data incomplete");
        }
        bufferSize = ntohl(*reinterpret_cast<uint32_t *>(sizeBytes));
    }
    std::string result(bufferSize, '\0');
    int pollres = poll(&pdf, 1, readTimeout_.count());
    ErrorCheck(pollres, "Polling Error on data receive");
    if (pollres == 0) {
        CloseConnection();
        throw std::system_error(errno, std::system_category(), "Timeout on reading data");
    }
    ssize_t bytes_read = recv(sock_, result.data(), bufferSize, 0);
    ErrorCheck(bytes_read, "Couldn't receive data");
    if (static_cast<size_t>(bytes_read) != bufferSize) {
        CloseConnection();
        throw std::runtime_error("Data incomplete");
    }
    return result;
}


void TcpConnect::CloseConnection() const {
    if (sock_ == -1) return;
    if (close(sock_) == -1) {
        sock_ = -1;
        throw std::system_error(errno, std::system_category(),
                                "Problems with closing the socket " + std::to_string(sock_));

    }
    sock_ = -1;
}

void TcpConnect::ErrorCheck(size_t checkValue, std::string &&what) const {
    if (checkValue == -1) {
        CloseConnection();
        throw std::system_error(errno, std::system_category(), what);
    }
}

const std::string &TcpConnect::GetIp() const {
    return ip_;
}

int TcpConnect::GetPort() const {
    return port_;
}

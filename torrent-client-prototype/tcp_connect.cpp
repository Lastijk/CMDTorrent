#include "tcp_connect.h"
#include "byte_tools.h"
#include "peer.h"

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
#include <limits>
#include <utility>

//using namespace std::chrono_literals;

TcpConnect::TcpConnect(std::string ip, int port, std::chrono::milliseconds connectTimeout,
                       std::chrono::milliseconds readTimeout) : ip_(ip), port_(port), connectTimeout_(connectTimeout),
                                                                readTimeout_(readTimeout), sock_(-1) {}

TcpConnect::TcpConnect() : port_(-1), sock_(-1), connectTimeout_(0), readTimeout_(0) {}

TcpConnect::TcpConnect(const Peer &peer) : ip_(peer.ip), port_(peer.port), connectTimeout_(5000), readTimeout_(5000), sock_(-1) {}

TcpConnect::~TcpConnect() {
    CloseConnection();
}

void TcpConnect::EstablishConnection() {
    std::lock_guard<std::recursive_mutex> lock(socketMutex_);

    const int created_socket = socket(AF_INET, SOCK_STREAM, 0);
    ErrorCheck(created_socket, "Couldn`t make a socket");
    sock_ = created_socket;
    const int sock = sock_.load();

    int f_values = fcntl(sock, F_GETFL, 0);
    ErrorCheck(fcntl(sock, F_SETFL, f_values | O_NONBLOCK), "Couldn`t set the mode to unblocked");

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port_);

    int convert_res = inet_pton(AF_INET, ip_.c_str(), &server.sin_addr);
    ErrorCheck(convert_res, "No valid address family");
    if (convert_res == 0) {
        CloseConnection();
        throw std::runtime_error("ip_ doesn`t contain a valid network address");
    }

    int connection_res = connect(sock, (const sockaddr *) &server, sizeof(server));
    if (connection_res == -1 && errno != EINPROGRESS) ErrorCheck(convert_res, "Couldn`t establish la connection");

    timeval timeout_time{};
    timeout_time.tv_usec = (connectTimeout_.count() % 1000) * 1000;
    timeout_time.tv_sec = connectTimeout_.count() / 1000;
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);

    int connect_res = select(sock + 1, NULL, &fdset, NULL, &timeout_time);
    switch (connect_res) {
        case -1:
            CloseConnection();
            throw std::system_error(errno, std::system_category(), "Couldn`t get a proper connection");
        case 0:
            CloseConnection();
            throw std::runtime_error("Timeout on connection");
        default:
            int so_error;
            socklen_t len = sizeof(so_error);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
            if (so_error != 0) {
                CloseConnection();
                throw std::system_error(so_error, std::system_category(), "Failed connection");
            }
    }

    ErrorCheck(fcntl(sock, F_SETFL, f_values & ~O_NONBLOCK), "Failed to rollback to blocking mode");
}

void TcpConnect::SendData(const std::string &data) const {
    std::lock_guard<std::recursive_mutex> lock(socketMutex_);

    const int sock = sock_.load();
    if (sock == -1) {
        throw std::runtime_error("Socket is closed");
    }
    size_t total_sent = 0;
    int bytes_sent = send(sock, data.c_str() + total_sent, data.size() - total_sent, 0);
    if (bytes_sent == -1 && errno != EINTR) {
        CloseConnection();
        throw std::system_error(errno, std::system_category(), "Couldn't send the data");
    }
    if (bytes_sent > 0) total_sent += static_cast<size_t>(bytes_sent);
    if (total_sent < data.size()) throw std::runtime_error("Not all data was sent");
}


std::string TcpConnect::ReceiveData(size_t bufferSize) const {
    std::lock_guard<std::recursive_mutex> lock(socketMutex_);

    const int sock = sock_.load();
    if (sock == -1) {
        throw std::runtime_error("Socket is closed");
    }

    auto waitForData = [this, sock](const std::string& what) {
        pollfd pdf{sock, POLLIN, 0};
        int pollres = poll(&pdf, 1, static_cast<int>(readTimeout_.count()));
        ErrorCheck(pollres, "Polling Error on " + what);
        if (pollres == 0) {
            CloseConnection();
            throw std::runtime_error("Timeout on " + what);
        }
    };

    if (bufferSize == 0) {
        char sizeBytes[4];
        waitForData("reading size");
        int bytes_size = recv(sock, sizeBytes, 4, 0);
        ErrorCheck(bytes_size, "Couldn't receive size of data because of an error");
        if (bytes_size == 0) {
            CloseConnection();
            throw std::runtime_error("Couldn`t receive size of data because peer closed the connection");
        }
        bufferSize = ntohl(*reinterpret_cast<uint32_t *>(sizeBytes));
    }
    std::string result(bufferSize, '\0');
    ssize_t received_bytes = 0;
    while(received_bytes < bufferSize && sock_.load() != -1){
        waitForData("reading data");
        ssize_t got_bytes = recv(sock, result.data() + received_bytes, bufferSize - received_bytes, 0);
        ErrorCheck(got_bytes, "Couldn't receive data because of an error");
        if (got_bytes == 0) {
            CloseConnection();
            throw std::runtime_error("Couldn`t receive data because peer closed the connection");
        }
        if(got_bytes > 0) received_bytes += got_bytes;
    }
    if (received_bytes != bufferSize) {
        CloseConnection();
        throw std::runtime_error("Data incomplete");
    }
    return result;
}


void TcpConnect::CloseConnection() const {
    std::lock_guard<std::recursive_mutex> lock(socketMutex_);

    const int old_sock = sock_.exchange(-1);
    if (old_sock == -1) return;
    close(old_sock);
}

void TcpConnect::ErrorCheck(int checkValue, std::string &&what) const {
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

#include "network.h"
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <thread>

// TcpConnection implementation (保持原样)
TcpConnection::TcpConnection(int fd) : fd_(fd), connected_(true) {}

TcpConnection::~TcpConnection() {
    close();
}

bool TcpConnection::send(const std::string& data) {
    if (!connected_) return false;
    
    // 先发送数据长度
    uint32_t len = htonl(data.length());
    if (::send(fd_, &len, sizeof(len), 0) != sizeof(len)) {
        connected_ = false;
        return false;
    }
    
    // 再发送数据内容
    size_t total_sent = 0;
    while (total_sent < data.length()) {
        ssize_t sent = ::send(fd_, data.c_str() + total_sent, 
                             data.length() - total_sent, 0);
        if (sent <= 0) {
            connected_ = false;
            return false;
        }
        total_sent += sent;
    }
    return true;
}

std::string TcpConnection::receive() {
    if (!connected_) return "";
    
    // 先接收数据长度
    uint32_t len;
    if (recv(fd_, &len, sizeof(len), MSG_WAITALL) != sizeof(len)) {
        connected_ = false;
        return "";
    }
    len = ntohl(len);
    
    if (len == 0 || len > 1024*1024) { // 防止过大的消息
        connected_ = false;
        return "";
    }
    
    // 再接收数据内容
    std::string data(len, 0);
    if (recv(fd_, &data[0], len, MSG_WAITALL) != len) {
        connected_ = false;
        return "";
    }
    
    return data;
}

void TcpConnection::close() {
    if (connected_) {
        ::close(fd_);
        connected_ = false;
    }
}

TcpServer::TcpServer(int port) : port_(port), listen_fd_(-1), epoll_fd_(-1), running_(false) {}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::start() {
    std::cout << "[TcpServer] Starting server on port " << port_ << std::endl;
    
    // 创建监听socket
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        std::cerr << "[TcpServer] Failed to create socket" << std::endl;
        return false;
    }
    
    // 设置端口重用
    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // 绑定地址
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    
    if (bind(listen_fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[TcpServer] Failed to bind to port " << port_ << ": " << strerror(errno) << std::endl;
        ::close(listen_fd_);
        return false;
    }
    
    // 开始监听
    if (listen(listen_fd_, 1024) < 0) {
        std::cerr << "[TcpServer] Failed to listen" << std::endl;
        ::close(listen_fd_);
        return false;
    }
    
    running_ = true;
    std::cout << "[TcpServer] ✓ Server started on port " << port_ << std::endl;
    return true;
}

void TcpServer::stop() {
    running_ = false;
    if (listen_fd_ >= 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
}

void TcpServer::setConnectionHandler(ConnectionHandler handler) {
    connection_handler_ = handler;
}

void TcpServer::run() {
    std::cout << "[TcpServer] Waiting for connections..." << std::endl;
    
    while (running_) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd_, (sockaddr*)&client_addr, &client_len);
        
        if (client_fd >= 0) {
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            std::cout << "[TcpServer] New connection from " << client_ip 
                     << ":" << ntohs(client_addr.sin_port) << std::endl;
            
            if (connection_handler_) {
                // 在新线程中处理连接
                std::thread([this, client_fd]() {
                    auto conn = std::make_shared<TcpConnection>(client_fd);
                    connection_handler_(conn);
                }).detach();
            } else {
                ::close(client_fd);
            }
        } else if (running_) {
            std::cerr << "[TcpServer] Accept failed: " << strerror(errno) << std::endl;
        }
    }
}

// TcpClient implementation (保持原样)
TcpClient::TcpClient() {}

TcpClient::~TcpClient() {
    disconnect();
}

bool TcpClient::connect(const std::string& host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;
    
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    
    if (::connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(fd);
        return false;
    }
    
    connection_ = std::make_shared<TcpConnection>(fd);
    return true;
}

void TcpClient::disconnect() {
    if (connection_) {
        connection_->close();
        connection_.reset();
    }
}

bool TcpClient::send(const std::string& data) {
    return connection_ && connection_->send(data);
}

std::string TcpClient::receive() {
    return connection_ ? connection_->receive() : "";
}

bool TcpClient::isConnected() const {
    return connection_ && connection_->isConnected();
}
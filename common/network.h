#ifndef NETWORK_H
#define NETWORK_H

#include <string>
#include <functional>
#include <memory>
#include <sys/epoll.h>
#include <netinet/in.h>

class TcpConnection {
public:
    TcpConnection(int fd);
    ~TcpConnection();
    
    bool send(const std::string& data);
    std::string receive();
    void close();
    int getFd() const { return fd_; }
    bool isConnected() const { return connected_; }
    
private:
    int fd_;
    bool connected_;
};

class TcpServer {
public:
    using ConnectionHandler = std::function<void(std::shared_ptr<TcpConnection>)>;
    
    TcpServer(int port);
    ~TcpServer();
    
    bool start();
    void stop();
    void setConnectionHandler(ConnectionHandler handler);
    void run();
    
private:
    int port_;
    int listen_fd_;
    int epoll_fd_;
    bool running_;
    ConnectionHandler connection_handler_;
    
    void handleNewConnection();
    void handleClientData(int client_fd);
};

class TcpClient {
public:
    TcpClient();
    ~TcpClient();
    
    bool connect(const std::string& host, int port);
    void disconnect();
    bool send(const std::string& data);
    std::string receive();
    bool isConnected() const;
    
private:
    std::shared_ptr<TcpConnection> connection_;
};

#endif
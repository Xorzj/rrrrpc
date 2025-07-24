#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>

int main() {
    std::cout << "=== Simple Registry Test ===" << std::endl;
    
    int port = 8080;
    int server_fd;
    
    // 创建socket
    std::cout << "1. Creating socket..." << std::endl;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "✗ Failed to create socket: " << strerror(errno) << std::endl;
        return 1;
    }
    std::cout << "✓ Socket created" << std::endl;
    
    // 设置端口重用
    std::cout << "2. Setting socket options..." << std::endl;
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "✗ Failed to set socket options: " << strerror(errno) << std::endl;
        close(server_fd);
        return 1;
    }
    std::cout << "✓ Socket options set" << std::endl;
    
    // 绑定地址
    std::cout << "3. Binding to port " << port << "..." << std::endl;
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "✗ Failed to bind to port " << port << ": " << strerror(errno) << std::endl;
        close(server_fd);
        return 1;
    }
    std::cout << "✓ Bound to port " << port << std::endl;
    
    // 开始监听
    std::cout << "4. Starting to listen..." << std::endl;
    if (listen(server_fd, 10) < 0) {
        std::cerr << "✗ Failed to listen: " << strerror(errno) << std::endl;
        close(server_fd);
        return 1;
    }
    std::cout << "✓ Listening on port " << port << std::endl;
    
    std::cout << "5. Registry server basic functionality works!" << std::endl;
    std::cout << "Press Ctrl+C to exit..." << std::endl;
    
    // 简单等待连接
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    std::cout << "Waiting for connections..." << std::endl;
    while (true) {
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd >= 0) {
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            std::cout << "Connection from " << client_ip << std::endl;
            close(client_fd);
        }
    }
    
    close(server_fd);
    return 0;
}
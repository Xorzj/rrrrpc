#include "../registry/registry_server.h"
#include <iostream>
#include <signal.h>

RegistryServer* g_registry = nullptr;

void signalHandler(int signum) {
    std::cout << "Shutting down registry server..." << std::endl;
    if (g_registry) {
        g_registry->stop();
    }
    exit(0);
}

int main() {
    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 创建并启动注册中心
    RegistryServer registry(8080);
    g_registry = &registry;
    
    std::cout << "Starting registry server on port 8080..." << std::endl;
    registry.start();
    
    return 0;
}
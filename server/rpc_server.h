#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include "../common/epoll_server.h"
#include "../proto/rpc_message.pb.h"
#include <functional>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <memory>
#include <string>
#include <iomanip>

class RpcServer {
public:
    using ServiceHandler = std::function<std::string(const std::string&)>;
    
    // 配置结构体 - 移到类的最前面
    struct Config {
        size_t thread_pool_size = std::thread::hardware_concurrency();
        size_t max_connections = 10000;
        int epoll_timeout = 1000; // ms
    };
    
    // 构造函数 - 提供两个版本避免默认参数问题
    RpcServer(int port, const std::string& registry_host, int registry_port);
    RpcServer(int port, const std::string& registry_host, int registry_port, const Config& config);
    ~RpcServer();
    
    // 禁用拷贝和移动
    RpcServer(const RpcServer&) = delete;
    RpcServer& operator=(const RpcServer&) = delete;
    RpcServer(RpcServer&&) = delete;
    RpcServer& operator=(RpcServer&&) = delete;
    
    // 注册服务方法
    void registerService(const std::string& service_name, 
                        const std::string& method_name,
                        ServiceHandler handler);
    
    // 启动和停止服务器
    void start();
    void stop();
    
    // 获取统计信息
    struct Stats {
        size_t active_connections = 0;
        size_t total_requests = 0;
        size_t failed_requests = 0;
        size_t thread_pool_queue_size = 0;
        size_t thread_pool_active_threads = 0;
        size_t bytes_received = 0;
        size_t bytes_sent = 0;
    };
    Stats getStats() const;
    
    // 打印统计信息
    void printStats() const;

private:
    // 服务器组件
    std::unique_ptr<EpollServer> epoll_server_;
    
    // 配置信息
    std::string registry_host_;
    int registry_port_;
    int server_port_;
    Config config_;
    
    // 服务处理器
    std::unordered_map<std::string, ServiceHandler> handlers_;
    
    // 线程和状态
    std::thread heartbeat_thread_;
    std::atomic<bool> running_;
    
    // 统计信息
    mutable std::atomic<size_t> total_requests_{0};
    mutable std::atomic<size_t> failed_requests_{0};
    
    // 私有方法
    void handleRpcRequest(int client_fd, const std::string& data);
    void sendErrorResponse(int client_fd, const std::string& request_id, 
                          int error_code, const std::string& error_msg);
    
    void registerToRegistry();
    void registerSingleService(const std::string& service_name);
    
    void sendHeartbeat();
    void sendHeartbeatForService(const std::string& service_name);
    
    std::string getLocalIP();
};

#endif
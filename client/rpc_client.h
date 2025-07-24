#ifndef RPC_CLIENT_H
#define RPC_CLIENT_H

#include "../common/connection_pool.h"
#include "../proto/rpc_message.pb.h"
#include <string>
#include <memory>
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <shared_mutex>
#include <future>
#include <vector>

class RpcClient {
public:
    // 配置结构体 - 移到类的最前面
    struct Config {
        // 连接池配置
        size_t max_connections_per_host = 10;
        size_t min_connections_per_host = 2;
        std::chrono::seconds connection_timeout{10};
        std::chrono::seconds idle_timeout{300};
        std::chrono::seconds acquire_timeout{5};
        
        // RPC配置
        std::chrono::milliseconds request_timeout{30000};
        int retry_count = 3;
        std::chrono::milliseconds retry_delay{100};
        
        // 服务发现缓存
        std::chrono::seconds service_cache_ttl{60};
    };
    
    // 构造函数 - 提供两个版本避免默认参数问题
    RpcClient(const std::string& registry_host, int registry_port);
    RpcClient(const std::string& registry_host, int registry_port, const Config& config);
    ~RpcClient();
    
    // 禁用拷贝和移动
    RpcClient(const RpcClient&) = delete;
    RpcClient& operator=(const RpcClient&) = delete;
    RpcClient(RpcClient&&) = delete;
    RpcClient& operator=(RpcClient&&) = delete;
    
    std::string call(const std::string& service_name, 
                    const std::string& method_name,
                    const std::string& request_data);
    
    // 异步调用
    std::future<std::string> callAsync(const std::string& service_name,
                                      const std::string& method_name,
                                      const std::string& request_data);
    
    // 预热连接
    void warmupConnections(const std::string& service_name, size_t count = 0);
    
    // 获取统计信息
    struct Stats {
        size_t total_calls = 0;
        size_t successful_calls = 0;
        size_t failed_calls = 0;
        size_t cache_hits = 0;
        size_t cache_misses = 0;
        double avg_response_time_ms = 0.0;
        ConnectionPool::Stats connection_stats;
    };
    Stats getStats() const;
    
    void stop();

private:
    struct ServiceEndpoint {
        std::string host;
        int port;
        std::chrono::steady_clock::time_point cached_time;
        
        bool isExpired(std::chrono::seconds ttl) const {
            auto now = std::chrono::steady_clock::now();
            return (now - cached_time) > ttl;
        }
    };
    
    std::string registry_host_;
    int registry_port_;
    Config config_;
    
    std::unique_ptr<ConnectionPool> connection_pool_;
    
    // 服务发现缓存
    std::unordered_map<std::string, std::vector<ServiceEndpoint>> service_cache_;
    mutable std::shared_mutex cache_mutex_;
    
    // 统计信息
    mutable std::atomic<size_t> total_calls_{0};
    mutable std::atomic<size_t> successful_calls_{0};
    mutable std::atomic<size_t> failed_calls_{0};
    mutable std::atomic<size_t> cache_hits_{0};
    mutable std::atomic<size_t> cache_misses_{0};
    mutable std::atomic<double> total_response_time_{0.0};
    
    // 负载均衡
    mutable std::unordered_map<std::string, std::atomic<size_t>> round_robin_counters_;
    
    std::vector<ServiceEndpoint> discoverService(const std::string& service_name);
    std::vector<ServiceEndpoint> discoverServiceFromRegistry(const std::string& service_name);
    ServiceEndpoint selectEndpoint(const std::vector<ServiceEndpoint>& endpoints);
    
    std::string callWithRetry(const ServiceEndpoint& endpoint,
                             const std::string& service_name,
                             const std::string& method_name,
                             const std::string& request_data);
    
    std::string callEndpoint(const ServiceEndpoint& endpoint,
                            const std::string& service_name,
                            const std::string& method_name,
                            const std::string& request_data);
    
    std::string generateRequestId();
};

#endif
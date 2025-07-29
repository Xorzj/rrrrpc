#include "rpc_client.h"
#include "../common/serializer.h"
#include "../common/network.h"
#include <iostream>
#include <random>
#include <future>
#include <thread>

// 默认构造函数
RpcClient::RpcClient(const std::string& registry_host, int registry_port)
    : RpcClient(registry_host, registry_port, Config{}) {
}

// 带配置的构造函数
RpcClient::RpcClient(const std::string& registry_host, int registry_port, const Config& config)
    : registry_host_(registry_host), registry_port_(registry_port), config_(config) {
    
    std::cout << "[RpcClient] Initializing with connection pool" << std::endl;
    std::cout << "[RpcClient] Registry: " << registry_host_ << ":" << registry_port_ << std::endl;
    std::cout << "[RpcClient] Max connections per host: " << config_.max_connections_per_host << std::endl;
    std::cout << "[RpcClient] Request timeout: " << config_.request_timeout.count() << "ms" << std::endl;
    std::cout << "[RpcClient] Retry count: " << config_.retry_count << std::endl;
    std::cout << "[RpcClient] Service cache TTL: " << config_.service_cache_ttl.count() << "s" << std::endl;
    
    // 创建连接池
    ConnectionPool::Config pool_config;
    pool_config.max_connections_per_host = config_.max_connections_per_host;
    pool_config.min_connections_per_host = config_.min_connections_per_host;
    pool_config.connection_timeout = config_.connection_timeout;
    pool_config.idle_timeout = config_.idle_timeout;
    pool_config.acquire_timeout = config_.acquire_timeout;
    
    connection_pool_ = std::make_unique<ConnectionPool>(pool_config);
}

RpcClient::~RpcClient() {
    stop();
}

void RpcClient::stop() {
    if (connection_pool_) {
        connection_pool_->stop();
        connection_pool_.reset();
    }
}

std::string RpcClient::call(const std::string& service_name, 
                           const std::string& method_name,
                           const std::string& request_data) {
    
    auto start_time = std::chrono::steady_clock::now();
    ++total_calls_;
    
    try {
        std::cout << "\n[RpcClient] === Starting RPC Call ===" << std::endl;
        std::cout << "[RpcClient] Service: " << service_name << "." << method_name << std::endl;
        
        // 1. 服务发现（带缓存）
        auto endpoints = discoverService(service_name);
        if (endpoints.empty()) {
            std::cout << "[RpcClient] ✗ No endpoints found for service: " << service_name << std::endl;
            ++failed_calls_;
            return "";
        }
        
        std::cout << "[RpcClient] ✓ Found " << endpoints.size() << " endpoint(s)" << std::endl;
        
        // 2. 负载均衡选择端点
        auto endpoint = selectEndpoint(endpoints);
        std::cout << "[RpcClient] Selected endpoint: " << endpoint.host << ":" << endpoint.port << std::endl;
        
        // 3. 调用（带重试）
        std::string result = callWithRetry(endpoint, service_name, method_name, request_data);
        
        if (!result.empty()) {
            ++successful_calls_;
            
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            // 更新平均响应时间
            double old_total = total_response_time_.load();
            double new_avg = (old_total * (successful_calls_.load() - 1) + duration.count()) / successful_calls_.load();
            total_response_time_.store(new_avg);
            
            std::cout << "[RpcClient] ✓ Call completed in " << duration.count() << "ms" << std::endl;
        } else {
            ++failed_calls_;
            std::cout << "[RpcClient] ✗ Call failed after retries" << std::endl;
        }
        
        std::cout << "[RpcClient] === RPC Call Completed ===" << std::endl;
        return result;
        
    } catch (const std::exception& e) {
        ++failed_calls_;
        std::cerr << "[RpcClient] ✗ Call failed with exception: " << e.what() << std::endl;
        return "";
    }
}

std::future<std::string> RpcClient::callAsync(const std::string& service_name,
                                             const std::string& method_name,
                                             const std::string& request_data) {
    return std::async(std::launch::async, [this, service_name, method_name, request_data]() {
        return call(service_name, method_name, request_data);
    });
}

void RpcClient::warmupConnections(const std::string& service_name, size_t count) {
    std::cout << "[RpcClient] Warming up connections for service: " << service_name << std::endl;
    
    try {
        auto endpoints = discoverService(service_name);
        for (const auto& endpoint : endpoints) {
            connection_pool_->warmup(endpoint.host, endpoint.port, count);
        }
    } catch (const std::exception& e) {
        std::cerr << "[RpcClient] Warmup failed: " << e.what() << std::endl;
    }
}

std::vector<RpcClient::ServiceEndpoint> RpcClient::discoverService(const std::string& service_name) {
    // 1. 检查缓存
    {
        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        auto it = service_cache_.find(service_name);
        if (it != service_cache_.end() && !it->second.empty()) {
            // 检查缓存是否过期
            if (!it->second[0].isExpired(config_.service_cache_ttl)) {
                ++cache_hits_;
                std::cout << "[RpcClient] ✓ Service cache hit for: " << service_name << std::endl;
                return it->second;
            }
        }
    }
    
    ++cache_misses_;
    std::cout << "[RpcClient] Service cache miss, discovering from registry: " << service_name << std::endl;
    
    // 2. 从注册中心获取
    auto endpoints = discoverServiceFromRegistry(service_name);
    
    // 3. 更新缓存
    if (!endpoints.empty()) {
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        service_cache_[service_name] = endpoints;
    }
    
    return endpoints;
}

std::vector<RpcClient::ServiceEndpoint> RpcClient::discoverServiceFromRegistry(const std::string& service_name) {
    std::vector<ServiceEndpoint> endpoints;
    
    try {
        // 使用连接池连接注册中心
        auto conn = connection_pool_->acquire(registry_host_, registry_port_);
        
        // 创建服务发现请求
        rpc::ServiceDiscoveryRequest request;
        request.set_service_name(service_name);
        
        // 发送请求
        std::string request_data = Serializer::serialize(request);
        if (!conn->send(request_data)) {
            throw std::runtime_error("Failed to send discovery request");
        }
        
        // 接收响应
        std::string response_data = conn->receive();
        if (response_data.empty()) {
            throw std::runtime_error("Empty response from registry");
        }
        
        // 解析响应
        rpc::ServiceDiscoveryResponse response;
        if (!Serializer::deserialize(response_data, &response)) {
            throw std::runtime_error("Failed to deserialize discovery response");
        }
        
        // 转换为内部格式
        auto now = std::chrono::steady_clock::now();
        for (int i = 0; i < response.services_size(); i++) {
            const auto& service = response.services(i);
            ServiceEndpoint endpoint;
            endpoint.host = service.host();
            endpoint.port = service.port();
            endpoint.cached_time = now;
            endpoints.push_back(endpoint);
        }
        
        // 归还连接
        connection_pool_->release(registry_host_, registry_port_, conn);
        
        std::cout << "[RpcClient] ✓ Discovered " << endpoints.size() 
                  << " endpoints for " << service_name << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[RpcClient] ✗ Service discovery failed: " << e.what() << std::endl;
    }
    
    return endpoints;
}

RpcClient::ServiceEndpoint RpcClient::selectEndpoint(const std::vector<ServiceEndpoint>& endpoints) {
    if (endpoints.empty()) {
        throw std::runtime_error("No endpoints available");
    }
    
    if (endpoints.size() == 1) {
        return endpoints[0];
    }
    
    // 轮询负载均衡
    std::string key = endpoints[0].host + ":" + std::to_string(endpoints[0].port);
    auto& counter = round_robin_counters_[key];
    size_t index = counter.fetch_add(1) % endpoints.size();
    
    return endpoints[index];
}

std::string RpcClient::callWithRetry(const ServiceEndpoint& endpoint,
                                    const std::string& service_name,
                                    const std::string& method_name,
                                    const std::string& request_data) {
    
    for (int attempt = 0; attempt < config_.retry_count; ++attempt) {
        try {
            if (attempt > 0) {
                std::cout << "[RpcClient] Retry attempt " << attempt + 1 << "/" << config_.retry_count << std::endl;
                std::this_thread::sleep_for(config_.retry_delay * attempt);
            }
            
            std::string result = callEndpoint(endpoint, service_name, method_name, request_data);
            if (!result.empty()) {
                return result;
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[RpcClient] Attempt " << (attempt + 1) << " failed: " << e.what() << std::endl;
        }
    }
    
    return "";
}

std::string RpcClient::callEndpoint(const ServiceEndpoint& endpoint,
                                   const std::string& service_name,
                                   const std::string& method_name,
                                   const std::string& request_data) {
    
    // 从连接池获取连接
    auto conn = connection_pool_->acquire(endpoint.host, endpoint.port);
    if (!conn) {
        throw std::runtime_error("Failed to acquire connection");
    }
    
    try {
        // 构造RPC请求
        rpc::RpcRequest request;
        request.set_service_name(service_name);
        request.set_method_name(method_name);
        request.set_request_data(request_data);
        request.set_request_id(generateRequestId());
        
        // 发送请求
        std::string serialized_request = Serializer::serialize(request);
        if (!conn->send(serialized_request)) {
            throw std::runtime_error("Failed to send RPC request");
        }
        
        // 接收响应
        std::string response_data = conn->receive();
        if (response_data.empty()) {
            throw std::runtime_error("Empty response from service");
        }
        
        // 解析响应
        rpc::RpcResponse response;
        if (!Serializer::deserialize(response_data, &response)) {
            throw std::runtime_error("Failed to deserialize RPC response");
        }
        
        if (response.error_code() != 0) {
            throw std::runtime_error("RPC error: " + response.error_msg());
        }
        
        // 归还连接
        connection_pool_->release(endpoint.host, endpoint.port, conn);
        return response.response_data();
        
    } catch (...) {
        // 发生异常时也要归还连接
        connection_pool_->release(endpoint.host, endpoint.port, conn);
        throw;
    }
}

RpcClient::Stats RpcClient::getStats() const {
    Stats stats;
    stats.total_calls = total_calls_.load();
    stats.successful_calls = successful_calls_.load();
    stats.failed_calls = failed_calls_.load();
    stats.cache_hits = cache_hits_.load();
    stats.cache_misses = cache_misses_.load();
    stats.avg_response_time_ms = total_response_time_.load();
    
    if (connection_pool_) {
        stats.connection_stats = connection_pool_->getStats();
    }
    
    return stats;
}

std::string RpcClient::generateRequestId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000000, 9999999);
    return std::to_string(dis(gen));
}
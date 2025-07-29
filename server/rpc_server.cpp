#include "rpc_server.h"
#include "../common/serializer.h"
#include "../common/network.h"
#include <iostream>
#include <thread>
#include <chrono>

// 默认构造函数
RpcServer::RpcServer(int port, const std::string& registry_host, int registry_port)
    : RpcServer(port, registry_host, registry_port, Config{}) {
}

// 带配置的构造函数
RpcServer::RpcServer(int port, const std::string& registry_host, int registry_port, const Config& config)
    : registry_host_(registry_host), registry_port_(registry_port), 
      server_port_(port), config_(config), running_(false) {
    
    std::cout << "[RpcServer] Creating RPC server on port " << port << std::endl;
    std::cout << "[RpcServer] Using epoll with " << config_.thread_pool_size << " worker threads" << std::endl;
    std::cout << "[RpcServer] Max connections: " << config_.max_connections << std::endl;
    std::cout << "[RpcServer] Registry: " << registry_host_ << ":" << registry_port_ << std::endl;
    
    // 创建epoll服务器配置
    EpollServer::Config epoll_config;
    epoll_config.thread_pool_size = config_.thread_pool_size;
    epoll_config.max_connections = config_.max_connections;
    epoll_config.epoll_timeout = config_.epoll_timeout;
    epoll_config.max_events = 1024;
    epoll_config.buffer_size = 4096;
    
    epoll_server_ = std::make_unique<EpollServer>(port, epoll_config);
    
    // 设置RPC请求处理器
    epoll_server_->setConnectionHandler([this](int client_fd, const std::string& data) {
        handleRpcRequest(client_fd, data);
    });
    
    // 设置连接关闭处理器
    epoll_server_->setConnectionCloseHandler([this](int client_fd) {
        std::cout << "[RpcServer] Client disconnected (fd=" << client_fd << ")" << std::endl;
    });
}

// 其余方法保持不变...
RpcServer::~RpcServer() {
    stop();
}

void RpcServer::registerService(const std::string& service_name, 
                               const std::string& method_name,
                               ServiceHandler handler) {
    std::string key = service_name + "." + method_name;
    handlers_[key] = handler;
    std::cout << "[RpcServer] ✓ Registered service: " << key << std::endl;
}

void RpcServer::start() {
    std::cout << "[RpcServer] Starting RPC server..." << std::endl;
    
    // 启动epoll服务器
    if (!epoll_server_->start()) {
        std::cerr << "[RpcServer] ✗ Failed to start epoll server" << std::endl;
        return;
    }
    
    running_ = true;
    
    // 注册到注册中心
    std::thread registration_thread([this]() {
        // 稍等一下确保服务器完全启动
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        registerToRegistry();
    });
    registration_thread.detach();
    
    // 启动心跳线程
    heartbeat_thread_ = std::thread([this]() {
        // 等待初始注册完成
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        while (running_) {
            sendHeartbeat();
            std::this_thread::sleep_for(std::chrono::seconds(15));
        }
    });
    
    std::cout << "[RpcServer] ✓ RPC server started successfully" << std::endl;
    std::cout << "[RpcServer] Listening on port " << server_port_ << " with epoll" << std::endl;
    std::cout << "[RpcServer] Worker threads: " << config_.thread_pool_size << std::endl;
    std::cout << "[RpcServer] Max connections: " << config_.max_connections << std::endl;
    std::cout << "[RpcServer] Registered " << handlers_.size() << " service methods" << std::endl;
    
    // 运行事件循环（阻塞）
    epoll_server_->run();
}

void RpcServer::stop() {
    if (!running_) return;
    
    std::cout << "[RpcServer] Stopping RPC server..." << std::endl;
    running_ = false;
    
    // 停止心跳线程
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
    
    // 停止epoll服务器
    if (epoll_server_) {
        epoll_server_->stop();
    }
    
    std::cout << "[RpcServer] ✓ RPC server stopped" << std::endl;
}

void RpcServer::handleRpcRequest(int client_fd, const std::string& data) {
    ++total_requests_;
    
    try {
        // 反序列化RPC请求
        rpc::RpcRequest request;
        if (!Serializer::deserialize(data, &request)) {
            std::cerr << "[RpcServer] ✗ Failed to deserialize request from fd=" << client_fd << std::endl;
            ++failed_requests_;
            sendErrorResponse(client_fd, "", -1, "Failed to deserialize request");
            return;
        }
        
        auto start_time = std::chrono::steady_clock::now();
        
        std::cout << "[RpcServer] Processing request: " << request.service_name() 
                  << "." << request.method_name() 
                  << " (id=" << request.request_id() << ")"
                  << " from fd=" << client_fd 
                  << " [Thread: " << std::this_thread::get_id() << "]" << std::endl;
        
        // 构造响应对象
        rpc::RpcResponse response;
        response.set_request_id(request.request_id());
        
        // 查找服务处理器
        std::string method_key = request.service_name() + "." + request.method_name();
        auto handler_it = handlers_.find(method_key);
        
        if (handler_it != handlers_.end()) {
            try {
                // 调用服务处理器
                std::string result = handler_it->second(request.request_data());
                
                // 设置成功响应
                response.set_error_code(0);
                response.set_response_data(result);
                
                auto end_time = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                
                std::cout << "[RpcServer] ✓ Request processed successfully in " 
                         << duration.count() << "ms"
                         << " [Thread: " << std::this_thread::get_id() << "]" << std::endl;
                
            } catch (const std::exception& e) {
                // 处理器执行异常
                response.set_error_code(-2);
                response.set_error_msg("Handler execution failed: " + std::string(e.what()));
                
                std::cerr << "[RpcServer] ✗ Handler execution failed: " << e.what() << std::endl;
                ++failed_requests_;
            }
        } else {
            // 方法未找到
            response.set_error_code(-1);
            response.set_error_msg("Method not found: " + method_key);
            
            std::cout << "[RpcServer] ✗ Method not found: " << method_key << std::endl;
            ++failed_requests_;
        }
        
        // 发送响应
        std::string response_data = Serializer::serialize(response);
        if (!epoll_server_->sendResponse(client_fd, response_data)) {
            std::cerr << "[RpcServer] ✗ Failed to send response to fd=" << client_fd << std::endl;
            ++failed_requests_;
        } else {
            std::cout << "[RpcServer] ✓ Response sent to fd=" << client_fd 
                     << " (size=" << response_data.size() << " bytes)" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[RpcServer] ✗ Request processing failed: " << e.what() << std::endl;
        ++failed_requests_;
        sendErrorResponse(client_fd, "", -3, "Internal server error: " + std::string(e.what()));
    }
}

void RpcServer::sendErrorResponse(int client_fd, const std::string& request_id, 
                                 int error_code, const std::string& error_msg) {
    try {
        rpc::RpcResponse response;
        response.set_request_id(request_id);
        response.set_error_code(error_code);
        response.set_error_msg(error_msg);
        
        std::string response_data = Serializer::serialize(response);
        epoll_server_->sendResponse(client_fd, response_data);
    } catch (...) {
        // 忽略发送错误响应时的异常
    }
}

RpcServer::Stats RpcServer::getStats() const {
    Stats stats;
    
    // 获取epoll服务器统计
    if (epoll_server_) {
        auto epoll_stats = epoll_server_->getStats();
        stats.active_connections = epoll_stats.active_connections;
        stats.bytes_received = epoll_stats.bytes_received;
        stats.bytes_sent = epoll_stats.bytes_sent;
    }
    
    // RPC层面的统计
    stats.total_requests = total_requests_.load();
    stats.failed_requests = failed_requests_.load();
    
    return stats;
}

void RpcServer::registerToRegistry() {
    std::cout << "[RpcServer] === Registering services to registry ===" << std::endl;
    
    try {
        // 注册EchoService
        registerSingleService("EchoService");
        
        // 短暂延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // 注册CalculatorService
        registerSingleService("CalculatorService");
        
        std::cout << "[RpcServer] ✓ All services registered successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[RpcServer] ✗ Service registration failed: " << e.what() << std::endl;
    }
}

void RpcServer::registerSingleService(const std::string& service_name) {
    std::cout << "[RpcServer] Registering service: " << service_name << std::endl;
    
    try {
        TcpClient client;
        if (!client.connect(registry_host_, registry_port_)) {
            throw std::runtime_error("Failed to connect to registry for " + service_name);
        }
        
        std::cout << "[RpcServer] ✓ Connected to registry for " << service_name << std::endl;
        
        // 创建注册请求
        rpc::ServiceRegisterRequest request;
        request.set_service_name(service_name);
        request.set_host("127.0.0.1");
        request.set_port(server_port_);
        
        std::cout << "[RpcServer] Registering " << service_name 
                  << " at 127.0.0.1:" << server_port_ << std::endl;
        
        // 发送注册请求
        std::string request_data = Serializer::serialize(request);
        if (!client.send(request_data)) {
            throw std::runtime_error("Failed to send registration request for " + service_name);
        }
        
        // 接收注册响应
        std::string response_data = client.receive();
        if (response_data.empty()) {
            throw std::runtime_error("Empty response from registry for " + service_name);
        }
        
        rpc::ServiceRegisterResponse response;
        if (!Serializer::deserialize(response_data, &response)) {
            throw std::runtime_error("Failed to deserialize registration response for " + service_name);
        }
        
        if (response.success()) {
            std::cout << "[RpcServer] ✓ " << service_name << " registration: " 
                     << response.message() << std::endl;
        } else {
            std::cout << "[RpcServer] ✗ " << service_name << " registration failed: " 
                     << response.message() << std::endl;
        }
        
        client.disconnect();
        
    } catch (const std::exception& e) {
        std::cerr << "[RpcServer] ✗ Failed to register " << service_name << ": " << e.what() << std::endl;
        throw;
    }
}

void RpcServer::sendHeartbeat() {
    if (!running_) return;
    
    try {
        std::cout << "[RpcServer] === Sending Heartbeat ===" << std::endl;
        
        // 为每个已注册的服务发送心跳
        sendHeartbeatForService("EchoService");
        sendHeartbeatForService("CalculatorService");
        
        std::cout << "[RpcServer] === Heartbeat Sent ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[RpcServer] ✗ Heartbeat failed: " << e.what() << std::endl;
    }
}

void RpcServer::sendHeartbeatForService(const std::string& service_name) {
    try {
        TcpClient client;
        if (!client.connect(registry_host_, registry_port_)) {
            std::cerr << "[RpcServer] Failed to connect to registry for heartbeat" << std::endl;
            return;
        }
        
        rpc::HeartbeatRequest request;
        request.set_service_name(service_name);
        request.set_host("127.0.0.1");
        request.set_port(server_port_);
        
        std::cout << "[RpcServer] 💓 Sending heartbeat for " << service_name 
                  << " from 127.0.0.1:" << server_port_ << std::endl;
        
        std::string request_data = Serializer::serialize(request);
        if (!client.send(request_data)) {
            std::cerr << "[RpcServer] Failed to send heartbeat for " << service_name << std::endl;
            return;
        }
        
        // 接收心跳响应
        std::string response_data = client.receive();
        rpc::HeartbeatResponse response;
        if (!response_data.empty() && Serializer::deserialize(response_data, &response)) {
            if (response.success()) {
                std::cout << "[RpcServer] ✓ Heartbeat acknowledged for " << service_name << std::endl;
            } else {
                std::cout << "[RpcServer] ⚠ Heartbeat failed for " << service_name 
                         << ": " << response.message() << std::endl;
            }
        } else {
            std::cout << "[RpcServer] ⚠ No heartbeat response for " << service_name << std::endl;
        }
        
        client.disconnect();
        
    } catch (const std::exception& e) {
        std::cerr << "[RpcServer] ✗ Heartbeat error for " << service_name << ": " << e.what() << std::endl;
    }
}

std::string RpcServer::getLocalIP() {
    // 简化实现，返回localhost
    return "127.0.0.1";
}

void RpcServer::printStats() const {
    auto stats = getStats();
    
    std::cout << "\n=== RPC Server Statistics ===" << std::endl;
    std::cout << "Active connections: " << stats.active_connections << std::endl;
    std::cout << "Total requests: " << stats.total_requests << std::endl;
    std::cout << "Failed requests: " << stats.failed_requests << std::endl;
    std::cout << "Success rate: " << std::fixed << std::setprecision(2) 
              << (stats.total_requests > 0 ? 
                  (100.0 * (stats.total_requests - stats.failed_requests) / stats.total_requests) : 0.0) 
              << "%" << std::endl;
    std::cout << "Bytes received: " << stats.bytes_received << std::endl;
    std::cout << "Bytes sent: " << stats.bytes_sent << std::endl;
    std::cout << "Registered services: " << handlers_.size() << std::endl;
    std::cout << "============================\n" << std::endl;
}
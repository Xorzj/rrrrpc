#include "registry_server.h"
#include "../common/serializer.h"
#include <iostream>
#include <algorithm>

RegistryServer::RegistryServer(int port) : server_(port), running_(false) {
    server_.setConnectionHandler([this](std::shared_ptr<TcpConnection> conn) {
        handleConnection(conn);
    });
}

RegistryServer::~RegistryServer() {
    stop();
}

void RegistryServer::start() {
    if (!server_.start()) {
        std::cerr << "Failed to start registry server" << std::endl;
        return;
    }
    
    running_ = true;
    
    // 启动心跳检查线程
    heartbeat_checker_ = std::thread([this]() {
        while (running_) {
            checkHeartbeat();
            std::this_thread::sleep_for(std::chrono::seconds(10)); // 每10秒检查一次
        }
    });
    
    std::cout << "[Registry] Registry server started with heartbeat monitoring" << std::endl;
    server_.run();
}

void RegistryServer::stop() {
    running_ = false;
    server_.stop();
    if (heartbeat_checker_.joinable()) {
        heartbeat_checker_.join();
    }
}

void RegistryServer::handleConnection(std::shared_ptr<TcpConnection> conn) {
    std::cout << "[Registry] === Handling Connection ===" << std::endl;
    
    std::string data = conn->receive();
    if (data.empty()) {
        std::cout << "[Registry] ✗ No data received from client" << std::endl;
        return;
    }
    
    std::cout << "[Registry] Received " << data.size() << " bytes" << std::endl;
    
    bool handled = false;
    
    // 1. 尝试解析为ServiceRegisterRequest
    if (!handled) {
        rpc::ServiceRegisterRequest request;
        if (Serializer::deserialize(data, &request)) {
            // 验证是否是有效的注册请求
            if (!request.service_name().empty() && 
                !request.host().empty() && 
                request.port() > 0) {
                
                std::cout << "[Registry] ✓ Valid ServiceRegisterRequest" << std::endl;
                std::cout << "[Registry] Service: '" << request.service_name() << "'" << std::endl;
                std::cout << "[Registry] Host: " << request.host() << std::endl;
                std::cout << "[Registry] Port: " << request.port() << std::endl;
                
                rpc::ServiceRegisterResponse response;
                handleServiceRegister(request, response);
                
                std::string response_data = Serializer::serialize(response);
                std::cout << "[Registry] Sending response (" << response_data.size() << " bytes)" << std::endl;
                
                if (conn->send(response_data)) {
                    std::cout << "[Registry] ✓ Response sent successfully" << std::endl;
                } else {
                    std::cout << "[Registry] ✗ Failed to send response" << std::endl;
                }
                handled = true;
            }
        }
    }
    
    // 2. 尝试解析为ServiceDiscoveryRequest
    if (!handled) {
        rpc::ServiceDiscoveryRequest request;
        if (Serializer::deserialize(data, &request)) {
            if (!request.service_name().empty()) {
                std::cout << "[Registry] ✓ Valid ServiceDiscoveryRequest" << std::endl;
                std::cout << "[Registry] Looking for service: '" << request.service_name() << "'" << std::endl;
                
                rpc::ServiceDiscoveryResponse response;
                handleServiceDiscovery(request, response);
                
                std::string response_data = Serializer::serialize(response);
                std::cout << "[Registry] Sending discovery response with " << response.services_size() << " services" << std::endl;
                
                if (conn->send(response_data)) {
                    std::cout << "[Registry] ✓ Discovery response sent" << std::endl;
                } else {
                    std::cout << "[Registry] ✗ Failed to send discovery response" << std::endl;
                }
                handled = true;
            }
        }
    }
    
    // 3. 尝试解析为HeartbeatRequest - 这是心跳处理！
    if (!handled) {
        rpc::HeartbeatRequest request;
        if (Serializer::deserialize(data, &request)) {
            if (!request.service_name().empty() && 
                !request.host().empty() && 
                request.port() > 0) {
                
                std::cout << "[Registry] ✓ Valid HeartbeatRequest" << std::endl;
                std::cout << "[Registry] 💓 Heartbeat from: " << request.service_name() 
                          << " at " << request.host() << ":" << request.port() << std::endl;
                
                rpc::HeartbeatResponse response;
                handleHeartbeat(request, response);
                
                std::string response_data = Serializer::serialize(response);
                conn->send(response_data);
                handled = true;
            }
        }
    }
    
    if (!handled) {
        std::cout << "[Registry] ✗ Unable to parse message as any valid request type" << std::endl;
    }
    
    std::cout << "[Registry] === Connection Handled ===" << std::endl;
}

void RegistryServer::handleServiceRegister(const rpc::ServiceRegisterRequest& request,
                                         rpc::ServiceRegisterResponse& response) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    
    std::cout << "[Registry] === Service Registration ===" << std::endl;
    std::cout << "[Registry] Registering service: '" << request.service_name() << "'" << std::endl;
    std::cout << "[Registry] Host: " << request.host() << std::endl;
    std::cout << "[Registry] Port: " << request.port() << std::endl;
    
    ServiceInstance instance;
    instance.host = request.host();
    instance.port = request.port();
    instance.last_heartbeat = std::chrono::steady_clock::now();
    
    // 检查是否已经存在相同的服务实例
    auto& instances = services_[request.service_name()];
    bool found = false;
    for (auto& existing : instances) {
        if (existing.host == instance.host && existing.port == instance.port) {
            existing.last_heartbeat = instance.last_heartbeat;
            found = true;
            std::cout << "[Registry] Updated existing instance heartbeat" << std::endl;
            break;
        }
    }
    
    if (!found) {
        instances.push_back(instance);
        std::cout << "[Registry] Added new service instance" << std::endl;
    }
    
    response.set_success(true);
    response.set_message("Service registered successfully");
    
    std::cout << "[Registry] Service '" << request.service_name() 
              << "' now has " << instances.size() << " instances" << std::endl;
    std::cout << "[Registry] Total services: " << services_.size() << std::endl;
    std::cout << "[Registry] === End Service Registration ===" << std::endl;
}

void RegistryServer::handleServiceDiscovery(const rpc::ServiceDiscoveryRequest& request,
                                          rpc::ServiceDiscoveryResponse& response) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    
    std::cout << "[Registry] === Service Discovery ===" << std::endl;
    std::cout << "[Registry] Looking up service: '" << request.service_name() << "'" << std::endl;
    std::cout << "[Registry] Total registered services: " << services_.size() << std::endl;
    
    // 打印所有已注册的服务
    for (const auto& service_pair : services_) {
        std::cout << "[Registry] Available service: '" << service_pair.first << "' with " 
                  << service_pair.second.size() << " instances" << std::endl;
        for (const auto& instance : service_pair.second) {
            std::cout << "[Registry]   - " << instance.host << ":" << instance.port 
                     << " (last heartbeat: " 
                     << std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::steady_clock::now() - instance.last_heartbeat).count() 
                     << "s ago)" << std::endl;
        }
    }
    
    auto it = services_.find(request.service_name());
    if (it != services_.end()) {
        std::cout << "[Registry] ✓ Found service '" << request.service_name() 
                  << "' with " << it->second.size() << " instances" << std::endl;
        
        for (const auto& instance : it->second) {
            // 只添加有效的实例（跳过host为空或port为0的）
            if (!instance.host.empty() && instance.port > 0) {
                auto* service_info = response.add_services();
                service_info->set_host(instance.host);
                service_info->set_port(instance.port);
                
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    instance.last_heartbeat.time_since_epoch());
                service_info->set_last_heartbeat(duration.count());
                
                std::cout << "[Registry] Added valid instance: " << instance.host << ":" << instance.port << std::endl;
            } else {
                std::cout << "[Registry] Skipped invalid instance: " << instance.host << ":" << instance.port << std::endl;
            }
        }
    } else {
        std::cout << "[Registry] ✗ Service '" << request.service_name() << "' not found" << std::endl;
    }
    
    std::cout << "[Registry] Response will contain " << response.services_size() << " services" << std::endl;
    std::cout << "[Registry] === End Service Discovery ===" << std::endl;
}

void RegistryServer::handleHeartbeat(const rpc::HeartbeatRequest& request,
                                   rpc::HeartbeatResponse& response) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    
    std::cout << "[Registry] === Processing Heartbeat ===" << std::endl;
    std::cout << "[Registry] 💓 Heartbeat from service: " << request.service_name() << std::endl;
    std::cout << "[Registry] 💓 Host: " << request.host() << ":" << request.port() << std::endl;
    
    auto it = services_.find(request.service_name());
    if (it != services_.end()) {
        for (auto& instance : it->second) {
            if (instance.host == request.host() && instance.port == request.port()) {
                instance.last_heartbeat = std::chrono::steady_clock::now();
                response.set_success(true);
                response.set_message("Heartbeat received");
                std::cout << "[Registry] ✓ Updated heartbeat for " << request.service_name() 
                         << " at " << request.host() << ":" << request.port() << std::endl;
                return;
            }
        }
    }
    
    // 如果没找到服务实例，返回失败
    response.set_success(false);
    response.set_message("Service instance not found");
    std::cout << "[Registry] ✗ Service instance not found for heartbeat: " 
             << request.service_name() << " at " << request.host() << ":" << request.port() << std::endl;
    std::cout << "[Registry] === End Processing Heartbeat ===" << std::endl;
}

void RegistryServer::checkHeartbeat() {
    std::lock_guard<std::mutex> lock(services_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(30); // 30秒超时
    
    std::cout << "[Registry] === Heartbeat Check ===" << std::endl;
    
    size_t total_removed = 0;
    
    for (auto& service_pair : services_) {
        const std::string& service_name = service_pair.first;
        auto& instances = service_pair.second;
        
        size_t before_count = instances.size();
        
        instances.erase(
            std::remove_if(instances.begin(), instances.end(),
                [now, timeout, &service_name](const ServiceInstance& instance) {
                    auto time_since_heartbeat = now - instance.last_heartbeat;
                    bool expired = time_since_heartbeat > timeout;
                    
                    if (expired) {
                        std::cout << "[Registry] 💀 Removing expired service: " << service_name 
                                 << " at " << instance.host << ":" << instance.port 
                                 << " (no heartbeat for " 
                                 << std::chrono::duration_cast<std::chrono::seconds>(time_since_heartbeat).count() 
                                 << "s)" << std::endl;
                    }
                    
                    return expired;
                }),
            instances.end()
        );
        
        size_t removed_count = before_count - instances.size();
        total_removed += removed_count;
        
        if (removed_count > 0) {
            std::cout << "[Registry] Service '" << service_name << "': removed " 
                     << removed_count << " expired instances, " 
                     << instances.size() << " remaining" << std::endl;
        }
    }
    
    // 移除没有实例的服务
    auto service_it = services_.begin();
    while (service_it != services_.end()) {
        if (service_it->second.empty()) {
            std::cout << "[Registry] Removing empty service: " << service_it->first << std::endl;
            service_it = services_.erase(service_it);
        } else {
            ++service_it;
        }
    }
    
    if (total_removed > 0) {
        std::cout << "[Registry] Heartbeat check completed: removed " << total_removed 
                 << " expired instances, " << services_.size() << " services remaining" << std::endl;
    }
    
    std::cout << "[Registry] === End Heartbeat Check ===" << std::endl;
}
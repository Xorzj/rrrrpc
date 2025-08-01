#include "distributed_registry_server.h"
#include "../common/serializer.h"
#include <iostream>
#include <chrono>

DistributedRegistryServer::DistributedRegistryServer(const std::string& node_id, 
                                                   const std::string& host, 
                                                   int client_port,
                                                   int raft_port,
                                                   const std::vector<RaftPeer>& peers)
    : node_id_(node_id), host_(host), client_port_(client_port), raft_port_(raft_port),
      running_(false) {
    
    // 创建状态机
    state_machine_ = std::make_shared<RegistryStateMachine>();
    
    // 创建Raft节点
    raft_node_ = std::make_unique<RaftNode>(node_id, host, raft_port, peers, state_machine_);
    
    // 创建客户端服务器
    client_server_ = std::make_unique<TcpServer>(client_port);
    client_server_->setConnectionHandler([this](std::shared_ptr<TcpConnection> conn) {
        handleClientConnection(conn);
    });
    
    std::cout << "[分布式注册中心] 节点 " << node_id_ << " 初始化完成" << std::endl;
    std::cout << "[分布式注册中心] 客户端端口: " << client_port_ << ", Raft端口: " << raft_port_ << std::endl;
}

DistributedRegistryServer::~DistributedRegistryServer() {
    stop();
}

void DistributedRegistryServer::start() {
    if (running_.load()) {
        return;
    }
    
    running_ = true;
    
    // 启动客户端服务器
    if (!client_server_->start()) {
        std::cerr << "[分布式注册中心] 启动客户端服务器失败" << std::endl;
        return;
    }
    
    // 启动清理线程
    cleanup_thread_ = std::thread(&DistributedRegistryServer::cleanupLoop, this);
    
    std::cout << "[分布式注册中心] 客户端服务器启动成功，端口: " << client_port_ << std::endl;
    
    // 在单独的线程中启动Raft节点
    std::thread raft_thread([this]() {
        raft_node_->start();
    });
    raft_thread.detach();
    
    // 等待一下让Raft节点启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "[分布式注册中心] 分布式注册中心启动完成" << std::endl;
    
    // 运行客户端服务器主循环
    client_server_->run();
}

void DistributedRegistryServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_ = false;
    
    // 停止Raft节点
    if (raft_node_) {
        raft_node_->stop();
    }
    
    // 停止客户端服务器
    if (client_server_) {
        client_server_->stop();
    }
    
    // 等待清理线程结束
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
    
    std::cout << "[分布式注册中心] 节点 " << node_id_ << " 已停止" << std::endl;
}

void DistributedRegistryServer::handleClientConnection(std::shared_ptr<TcpConnection> conn) {
    std::cout << "[分布式注册中心] === 处理客户端连接 ===" << std::endl;
    
    std::string data = conn->receive();
    if (data.empty()) {
        std::cout << "[分布式注册中心] ✗ 未收到客户端数据" << std::endl;
        return;
    }
    
    std::cout << "[分布式注册中心] 收到 " << data.size() << " 字节数据" << std::endl;
    
    bool handled = false;
    
    // 1. 尝试解析为服务注册请求
    if (!handled) {
        rpc::ServiceRegisterRequest request;
        if (Serializer::deserialize(data, &request)) {
            if (!request.service_name().empty() && 
                !request.host().empty() && 
                request.port() > 0) {
                
                std::cout << "[分布式注册中心] ✓ 有效的服务注册请求" << std::endl;
                std::cout << "[分布式注册中心] 服务: '" << request.service_name() << "'" << std::endl;
                std::cout << "[分布式注册中心] 主机: " << request.host() << std::endl;
                std::cout << "[分布式注册中心] 端口: " << request.port() << std::endl;
                
                rpc::ServiceRegisterResponse response;
                handleServiceRegister(request, response);
                
                std::string response_data = Serializer::serialize(response);
                std::cout << "[分布式注册中心] 发送响应 (" << response_data.size() << " 字节)" << std::endl;
                
                if (conn->send(response_data)) {
                    std::cout << "[分布式注册中心] ✓ 响应发送成功" << std::endl;
                } else {
                    std::cout << "[分布式注册中心] ✗ 响应发送失败" << std::endl;
                }
                handled = true;
            }
        }
    }
    
    // 2. 尝试解析为服务发现请求
    if (!handled) {
        rpc::ServiceDiscoveryRequest request;
        if (Serializer::deserialize(data, &request)) {
            if (!request.service_name().empty()) {
                std::cout << "[分布式注册中心] ✓ 有效的服务发现请求" << std::endl;
                std::cout << "[分布式注册中心] 查找服务: '" << request.service_name() << "'" << std::endl;
                
                rpc::ServiceDiscoveryResponse response;
                handleServiceDiscovery(request, response);
                
                std::string response_data = Serializer::serialize(response);
                std::cout << "[分布式注册中心] 发送发现响应，包含 " << response.services_size() << " 个服务" << std::endl;
                
                if (conn->send(response_data)) {
                    std::cout << "[分布式注册中心] ✓ 发现响应发送成功" << std::endl;
                } else {
                    std::cout << "[分布式注册中心] ✗ 发现响应发送失败" << std::endl;
                }
                handled = true;
            }
        }
    }
    
    // 3. 尝试解析为心跳请求
    if (!handled) {
        rpc::HeartbeatRequest request;
        if (Serializer::deserialize(data, &request)) {
            if (!request.service_name().empty() && 
                !request.host().empty() && 
                request.port() > 0) {
                
                std::cout << "[分布式注册中心] ✓ 有效的心跳请求" << std::endl;
                std::cout << "[分布式注册中心] 💓 来自服务的心跳: " << request.service_name() 
                          << " at " << request.host() << ":" << request.port() << std::endl;
                
                rpc::HeartbeatResponse response;
                handleHeartbeat(request, response);
                
                std::string response_data = Serializer::serialize(response);
                conn->send(response_data);
                handled = true;
            }
        }
    }
    
    // 4. 尝试解析为集群状态请求
    if (!handled) {
        rpc::ClusterStatusRequest request;
        if (Serializer::deserialize(data, &request)) {
            std::cout << "[分布式注册中心] ✓ 集群状态请求" << std::endl;
            
            rpc::ClusterStatusResponse response;
            handleClusterStatus(response);
            
            std::string response_data = Serializer::serialize(response);
            conn->send(response_data);
            handled = true;
        }
    }
    
    if (!handled) {
        std::cout << "[分布式注册中心] ✗ 无法解析消息为任何有效的请求类型" << std::endl;
    }
    
    std::cout << "[分布式注册中心] === 客户端连接处理完成 ===" << std::endl;
}

void DistributedRegistryServer::handleServiceRegister(const rpc::ServiceRegisterRequest& request,
                                                     rpc::ServiceRegisterResponse& response) {
    std::cout << "[分布式注册中心] === 服务注册 ===" << std::endl;
    
    // 检查是否为Leader
    if (!raft_node_->isLeader()) {
        response.set_success(false);
        response.set_message("不是Leader节点，请联系Leader: " + raft_node_->getLeaderId());
        std::cout << "[分布式注册中心] ✗ 非Leader节点拒绝注册请求" << std::endl;
        return;
    }
    
    // 提交服务注册操作到Raft集群
    bool success = proposeServiceOperation("register", 
                                          request.service_name(),
                                          request.host(),
                                          request.port());
    
    if (success) {
        response.set_success(true);
        response.set_message("服务注册成功");
        std::cout << "[分布式注册中心] ✓ 服务注册提议成功: " << request.service_name() << std::endl;
    } else {
        response.set_success(false);
        response.set_message("服务注册失败");
        std::cout << "[分布式注册中心] ✗ 服务注册提议失败: " << request.service_name() << std::endl;
    }
    
    std::cout << "[分布式注册中心] === 服务注册结束 ===" << std::endl;
}

void DistributedRegistryServer::handleServiceDiscovery(const rpc::ServiceDiscoveryRequest& request,
                                                      rpc::ServiceDiscoveryResponse& response) {
    std::cout << "[分布式注册中心] === 服务发现 ===" << std::endl;
    std::cout << "[分布式注册中心] 查找服务: '" << request.service_name() << "'" << std::endl;
    
    // 从状态机获取服务实例
    auto instances = state_machine_->getServiceInstances(request.service_name());
    
    std::cout << "[分布式注册中心] 找到 " << instances.size() << " 个服务实例" << std::endl;
    
    for (const auto& instance : instances) {
        if (!instance.host.empty() && instance.port > 0) {
            auto* service_info = response.add_services();
            service_info->set_host(instance.host);
            service_info->set_port(instance.port);
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                instance.last_heartbeat.time_since_epoch());
            service_info->set_last_heartbeat(duration.count());
            
            std::cout << "[分布式注册中心] 添加有效实例: " << instance.host << ":" << instance.port << std::endl;
        }
    }
    
    std::cout << "[分布式注册中心] 响应将包含 " << response.services_size() << " 个服务" << std::endl;
    std::cout << "[分布式注册中心] === 服务发现结束 ===" << std::endl;
}

void DistributedRegistryServer::handleHeartbeat(const rpc::HeartbeatRequest& request,
                                               rpc::HeartbeatResponse& response) {
    std::cout << "[分布式注册中心] === 处理心跳 ===" << std::endl;
    
    // 检查是否为Leader
    if (!raft_node_->isLeader()) {
        response.set_success(false);
        response.set_message("不是Leader节点，请联系Leader: " + raft_node_->getLeaderId());
        return;
    }
    
    // 提交心跳操作到Raft集群
    bool success = proposeServiceOperation("heartbeat",
                                          request.service_name(),
                                          request.host(),
                                          request.port());
    
    if (success) {
        response.set_success(true);
        response.set_message("心跳接收成功");
        std::cout << "[分布式注册中心] ✓ 心跳处理成功: " << request.service_name() << std::endl;
    } else {
        response.set_success(false);
        response.set_message("心跳处理失败");
        std::cout << "[分布式注册中心] ✗ 心跳处理失败: " << request.service_name() << std::endl;
    }
    
    std::cout << "[分布式注册中心] === 心跳处理结束 ===" << std::endl;
}

void DistributedRegistryServer::handleClusterStatus(rpc::ClusterStatusResponse& response) {
    std::cout << "[分布式注册中心] 获取集群状态" << std::endl;
    raft_node_->getClusterStatus(response);
}

bool DistributedRegistryServer::proposeServiceOperation(const std::string& operation_type,
                                                       const std::string& service_name,
                                                       const std::string& host,
                                                       int port) {
    // 创建服务操作
    rpc::ServiceOperation operation;
    operation.set_operation_type(operation_type);
    operation.set_service_name(service_name);
    operation.set_host(host);
    operation.set_port(port);
    operation.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    
    // 序列化操作
    std::string operation_data = operation.SerializeAsString();
    
    // 提交到Raft集群
    return raft_node_->proposeLogEntry("service_operation", operation_data);
}

void DistributedRegistryServer::cleanupLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(30)); // 每30秒清理一次
        
        if (!running_.load()) break;
        
        std::cout << "[分布式注册中心] 执行定期清理..." << std::endl;
        state_machine_->removeExpiredServices();
        
        std::cout << "[分布式注册中心] 当前服务数量: " << state_machine_->getServiceCount() << std::endl;
    }
}

bool DistributedRegistryServer::isLeader() const {
    return raft_node_->isLeader();
}

std::string DistributedRegistryServer::getLeaderId() const {
    return raft_node_->getLeaderId();
}
#include "../client/rpc_client.h"
#include "../proto/rpc_message.pb.h"
#include "../proto/distributed_registry.pb.h"
#include "../common/serializer.h"
#include "../common/network.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

class DistributedRegistryTestClient {
public:
    DistributedRegistryTestClient(const std::string& host, int port) 
        : host_(host), port_(port) {}
    
    // 测试服务注册
    bool testServiceRegister(const std::string& service_name, const std::string& host, int port) {
        std::cout << "[测试] 注册服务: " << service_name << " at " << host << ":" << port << std::endl;
        
        rpc::ServiceRegisterRequest request;
        request.set_service_name(service_name);
        request.set_host(host);
        request.set_port(port);
        
        TcpClient client;
        if (!client.connect(host_, port_)) {
            std::cout << "[测试] 连接注册中心失败" << std::endl;
            return false;
        }
        
        std::string request_data = Serializer::serialize(request);
        if (!client.send(request_data)) {
            std::cout << "[测试] 发送注册请求失败" << std::endl;
            return false;
        }
        
        std::string response_data = client.receive();
        if (response_data.empty()) {
            std::cout << "[测试] 接收注册响应失败" << std::endl;
            return false;
        }
        
        rpc::ServiceRegisterResponse response;
        if (!Serializer::deserialize(response_data, &response)) {
            std::cout << "[测试] 解析注册响应失败" << std::endl;
            return false;
        }
        
        if (response.success()) {
            std::cout << "[测试] ✓ 服务注册成功: " << response.message() << std::endl;
            return true;
        } else {
            std::cout << "[测试] ✗ 服务注册失败: " << response.message() << std::endl;
            return false;
        }
    }
    
    // 测试服务发现
    bool testServiceDiscovery(const std::string& service_name) {
        std::cout << "[测试] 发现服务: " << service_name << std::endl;
        
        rpc::ServiceDiscoveryRequest request;
        request.set_service_name(service_name);
        
        TcpClient client;
        if (!client.connect(host_, port_)) {
            std::cout << "[测试] 连接注册中心失败" << std::endl;
            return false;
        }
        
        std::string request_data = Serializer::serialize(request);
        if (!client.send(request_data)) {
            std::cout << "[测试] 发送发现请求失败" << std::endl;
            return false;
        }
        
        std::string response_data = client.receive();
        if (response_data.empty()) {
            std::cout << "[测试] 接收发现响应失败" << std::endl;
            return false;
        }
        
        rpc::ServiceDiscoveryResponse response;
        if (!Serializer::deserialize(response_data, &response)) {
            std::cout << "[测试] 解析发现响应失败" << std::endl;
            return false;
        }
        
        std::cout << "[测试] 找到 " << response.services_size() << " 个服务实例:" << std::endl;
        for (const auto& service : response.services()) {
            std::cout << "[测试]   - " << service.host() << ":" << service.port() 
                      << " (心跳: " << service.last_heartbeat() << ")" << std::endl;
        }
        
        return response.services_size() > 0;
    }
    
    // 测试心跳
    bool testHeartbeat(const std::string& service_name, const std::string& host, int port) {
        std::cout << "[测试] 发送心跳: " << service_name << " at " << host << ":" << port << std::endl;
        
        rpc::HeartbeatRequest request;
        request.set_service_name(service_name);
        request.set_host(host);
        request.set_port(port);
        
        TcpClient client;
        if (!client.connect(host_, port_)) {
            std::cout << "[测试] 连接注册中心失败" << std::endl;
            return false;
        }
        
        std::string request_data = Serializer::serialize(request);
        if (!client.send(request_data)) {
            std::cout << "[测试] 发送心跳请求失败" << std::endl;
            return false;
        }
        
        std::string response_data = client.receive();
        if (response_data.empty()) {
            std::cout << "[测试] 接收心跳响应失败" << std::endl;
            return false;
        }
        
        rpc::HeartbeatResponse response;
        if (!Serializer::deserialize(response_data, &response)) {
            std::cout << "[测试] 解析心跳响应失败" << std::endl;
            return false;
        }
        
        if (response.success()) {
            std::cout << "[测试] ✓ 心跳成功: " << response.message() << std::endl;
            return true;
        } else {
            std::cout << "[测试] ✗ 心跳失败: " << response.message() << std::endl;
            return false;
        }
    }
    
    // 测试集群状态
    bool testClusterStatus() {
        std::cout << "[测试] 获取集群状态" << std::endl;
        
        rpc::ClusterStatusRequest request;
        
        TcpClient client;
        if (!client.connect(host_, port_)) {
            std::cout << "[测试] 连接注册中心失败" << std::endl;
            return false;
        }
        
        std::string request_data = Serializer::serialize(request);
        if (!client.send(request_data)) {
            std::cout << "[测试] 发送集群状态请求失败" << std::endl;
            return false;
        }
        
        std::string response_data = client.receive();
        if (response_data.empty()) {
            std::cout << "[测试] 接收集群状态响应失败" << std::endl;
            return false;
        }
        
        rpc::ClusterStatusResponse response;
        if (!Serializer::deserialize(response_data, &response)) {
            std::cout << "[测试] 解析集群状态响应失败" << std::endl;
            return false;
        }
        
        std::cout << "[测试] 集群状态:" << std::endl;
        std::cout << "[测试] Leader: " << response.leader_id() << std::endl;
        std::cout << "[测试] 节点数量: " << response.nodes_size() << std::endl;
        
        for (const auto& node : response.nodes()) {
            std::cout << "[测试]   节点: " << node.node_id() 
                      << " (" << node.role() << ") "
                      << "at " << node.host() << ":" << node.port()
                      << " term=" << node.term() << std::endl;
        }
        
        return true;
    }

private:
    std::string host_;
    int port_;
};

void runBasicTests() {
    std::cout << "\n=== 基础功能测试 ===" << std::endl;
    
    // 测试三个节点
    std::vector<std::pair<std::string, int>> registry_nodes = {
        {"127.0.0.1", 8080},
        {"127.0.0.1", 8081},
        {"127.0.0.1", 8082}
    };
    
    // 等待集群启动
    std::cout << "等待集群启动..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // 尝试连接每个节点进行测试
    for (int i = 0; i < registry_nodes.size(); ++i) {
        std::cout << "\n--- 测试节点 " << (i+1) << " ---" << std::endl;
        
        DistributedRegistryTestClient client(registry_nodes[i].first, registry_nodes[i].second);
        
        // 测试集群状态
        client.testClusterStatus();
        
        // 测试服务注册
        std::string service_name = "test-service-" + std::to_string(i+1);
        bool register_success = client.testServiceRegister(service_name, "127.0.0.1", 9000 + i);
        
        if (register_success) {
            // 测试服务发现
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            client.testServiceDiscovery(service_name);
            
            // 测试心跳
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            client.testHeartbeat(service_name, "127.0.0.1", 9000 + i);
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void runLoadTest() {
    std::cout << "\n=== 负载测试 ===" << std::endl;
    
    DistributedRegistryTestClient client("127.0.0.1", 8080);
    
    // 注册多个服务
    for (int i = 0; i < 10; ++i) {
        std::string service_name = "load-test-service-" + std::to_string(i);
        client.testServiceRegister(service_name, "127.0.0.1", 10000 + i);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 发现所有服务
    for (int i = 0; i < 10; ++i) {
        std::string service_name = "load-test-service-" + std::to_string(i);
        client.testServiceDiscovery(service_name);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // 持续发送心跳
    std::cout << "发送持续心跳..." << std::endl;
    for (int round = 0; round < 5; ++round) {
        for (int i = 0; i < 10; ++i) {
            std::string service_name = "load-test-service-" + std::to_string(i);
            client.testHeartbeat(service_name, "127.0.0.1", 10000 + i);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main(int argc, char* argv[]) {
    std::cout << "分布式注册中心测试客户端" << std::endl;
    std::cout << "确保分布式注册中心集群已经启动" << std::endl;
    
    try {
        // 运行基础测试
        runBasicTests();
        
        // 运行负载测试
        runLoadTest();
        
        std::cout << "\n=== 测试完成 ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "测试过程中发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
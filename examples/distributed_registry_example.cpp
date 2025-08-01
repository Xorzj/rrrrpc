#include "../registry/distributed_registry_server.h"
#include <iostream>
#include <signal.h>
#include <fstream>
#include <json/json.h>

DistributedRegistryServer* g_registry = nullptr;

void signalHandler(int signum) {
    std::cout << "正在关闭分布式注册中心服务器..." << std::endl;
    if (g_registry) {
        g_registry->stop();
    }
    exit(0);
}

// 从配置文件加载集群配置
bool loadClusterConfig(const std::string& config_file, 
                      std::string& node_id,
                      std::string& host,
                      int& client_port,
                      int& raft_port,
                      std::vector<RaftPeer>& peers) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "无法打开配置文件: " << config_file << std::endl;
        return false;
    }
    
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(file, root)) {
        std::cerr << "解析配置文件失败: " << reader.getFormattedErrorMessages() << std::endl;
        return false;
    }
    
    // 获取节点配置
    const Json::Value& nodes = root["cluster"]["nodes"];
    if (nodes.empty()) {
        std::cerr << "配置文件中没有找到节点信息" << std::endl;
        return false;
    }
    
    // 默认使用第一个节点作为当前节点
    const Json::Value& current_node = nodes[0];
    node_id = current_node["node_id"].asString();
    host = current_node["host"].asString();
    client_port = current_node["client_port"].asInt();
    raft_port = current_node["raft_port"].asInt();
    
    // 加载其他节点作为peers
    for (int i = 1; i < nodes.size(); ++i) {
        const Json::Value& peer_node = nodes[i];
        
        RaftPeer peer;
        peer.node_id = peer_node["node_id"].asString();
        peer.host = peer_node["host"].asString();
        peer.port = peer_node["raft_port"].asInt();
        peer.next_index = 1;
        peer.match_index = 0;
        
        peers.push_back(peer);
    }
    
    return true;
}

int main(int argc, char* argv[]) {
    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::string config_file = "config/distributed_registry_config.json";
    int node_index = 0; // 默认启动第一个节点
    
    // 解析命令行参数
    if (argc >= 2) {
        node_index = std::atoi(argv[1]);
        if (node_index < 0 || node_index > 2) {
            std::cerr << "节点索引必须在0-2之间" << std::endl;
            return 1;
        }
    }
    
    if (argc >= 3) {
        config_file = argv[2];
    }
    
    std::cout << "启动分布式注册中心节点 " << node_index << std::endl;
    std::cout << "使用配置文件: " << config_file << std::endl;
    
    // 加载配置
    std::string node_id, host;
    int client_port, raft_port;
    std::vector<RaftPeer> peers;
    
    // 简化版本：直接硬编码配置
    std::vector<std::tuple<std::string, std::string, int, int>> node_configs = {
        {"registry-node-1", "127.0.0.1", 8080, 9080},
        {"registry-node-2", "127.0.0.1", 8081, 9081}, 
        {"registry-node-3", "127.0.0.1", 8082, 9082}
    };
    
    if (node_index >= node_configs.size()) {
        std::cerr << "无效的节点索引: " << node_index << std::endl;
        return 1;
    }
    
    // 设置当前节点配置
    auto current_config = node_configs[node_index];
    node_id = std::get<0>(current_config);
    host = std::get<1>(current_config);
    client_port = std::get<2>(current_config);
    raft_port = std::get<3>(current_config);
    
    // 设置peer配置
    for (int i = 0; i < node_configs.size(); ++i) {
        if (i != node_index) {
            RaftPeer peer;
            peer.node_id = std::get<0>(node_configs[i]);
            peer.host = std::get<1>(node_configs[i]);
            peer.port = std::get<3>(node_configs[i]); // raft端口
            peer.next_index = 1;
            peer.match_index = 0;
            peers.push_back(peer);
        }
    }
    
    std::cout << "节点配置:" << std::endl;
    std::cout << "  节点ID: " << node_id << std::endl;
    std::cout << "  主机: " << host << std::endl;
    std::cout << "  客户端端口: " << client_port << std::endl;
    std::cout << "  Raft端口: " << raft_port << std::endl;
    std::cout << "  Peer数量: " << peers.size() << std::endl;
    
    for (const auto& peer : peers) {
        std::cout << "    Peer: " << peer.node_id << " at " << peer.host << ":" << peer.port << std::endl;
    }
    
    try {
        // 创建并启动分布式注册中心
        DistributedRegistryServer registry(node_id, host, client_port, raft_port, peers);
        g_registry = &registry;
        
        std::cout << "正在启动分布式注册中心服务器..." << std::endl;
        registry.start();
        
    } catch (const std::exception& e) {
        std::cerr << "启动分布式注册中心失败: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
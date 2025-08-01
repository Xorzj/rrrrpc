#ifndef DISTRIBUTED_REGISTRY_SERVER_H
#define DISTRIBUTED_REGISTRY_SERVER_H

#include "raft_node.h"
#include "raft_state_machine.h"
#include "../common/network.h"
#include "../proto/rpc_message.pb.h"
#include "../proto/distributed_registry.pb.h"
#include <memory>
#include <thread>
#include <atomic>

class DistributedRegistryServer {
public:
    DistributedRegistryServer(const std::string& node_id, 
                             const std::string& host, 
                             int client_port,
                             int raft_port,
                             const std::vector<RaftPeer>& peers);
    ~DistributedRegistryServer();
    
    void start();
    void stop();
    
    // 客户端接口
    bool isLeader() const;
    std::string getLeaderId() const;
    
private:
    std::string node_id_;
    std::string host_;
    int client_port_;
    int raft_port_;
    
    // Raft组件
    std::shared_ptr<RegistryStateMachine> state_machine_;
    std::unique_ptr<RaftNode> raft_node_;
    
    // 客户端服务
    std::unique_ptr<TcpServer> client_server_;
    std::atomic<bool> running_;
    std::thread cleanup_thread_;
    
    // 处理客户端连接
    void handleClientConnection(std::shared_ptr<TcpConnection> conn);
    
    // 处理各种客户端请求
    void handleServiceRegister(const rpc::ServiceRegisterRequest& request,
                              rpc::ServiceRegisterResponse& response);
    void handleServiceDiscovery(const rpc::ServiceDiscoveryRequest& request,
                               rpc::ServiceDiscoveryResponse& response);
    void handleHeartbeat(const rpc::HeartbeatRequest& request,
                        rpc::HeartbeatResponse& response);
    void handleClusterStatus(rpc::ClusterStatusResponse& response);
    
    // 定期清理过期服务
    void cleanupLoop();
    
    // 将操作提交到Raft集群
    bool proposeServiceOperation(const std::string& operation_type,
                                const std::string& service_name,
                                const std::string& host,
                                int port);
};

#endif
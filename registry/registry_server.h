#ifndef REGISTRY_SERVER_H
#define REGISTRY_SERVER_H

#include "../common/network.h"
#include "../proto/rpc_message.pb.h"
#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>

struct ServiceInstance {
    std::string host;
    int port;
    std::chrono::steady_clock::time_point last_heartbeat;
};

class RegistryServer {
public:
    RegistryServer(int port);
    ~RegistryServer();
    
    void start();
    void stop();
    
private:
    TcpServer server_;
    std::unordered_map<std::string, std::vector<ServiceInstance>> services_;
    std::mutex services_mutex_;
    std::thread heartbeat_checker_;
    bool running_;
    
    void handleConnection(std::shared_ptr<TcpConnection> conn);
    void handleServiceRegister(const rpc::ServiceRegisterRequest& request,
                              rpc::ServiceRegisterResponse& response);
    void handleServiceDiscovery(const rpc::ServiceDiscoveryRequest& request,
                               rpc::ServiceDiscoveryResponse& response);
    void handleHeartbeat(const rpc::HeartbeatRequest& request,
                        rpc::HeartbeatResponse& response);
    void checkHeartbeat();
};

#endif
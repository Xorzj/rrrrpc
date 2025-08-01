#ifndef RAFT_STATE_MACHINE_H
#define RAFT_STATE_MACHINE_H

#include "../proto/distributed_registry.pb.h"
#include "../proto/rpc_message.pb.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <chrono>

struct ServiceInstance {
    std::string host;
    int port;
    std::chrono::steady_clock::time_point last_heartbeat;
};

// Raft状态机接口
class RaftStateMachine {
public:
    virtual ~RaftStateMachine() = default;
    
    // 应用日志条目到状态机
    virtual void applyLogEntry(const rpc::LogEntry& entry) = 0;
    
    // 获取状态机快照
    virtual std::string getSnapshot() = 0;
    
    // 从快照恢复状态机
    virtual void restoreFromSnapshot(const std::string& snapshot) = 0;
};

// 注册中心状态机实现
class RegistryStateMachine : public RaftStateMachine {
public:
    RegistryStateMachine();
    
    void applyLogEntry(const rpc::LogEntry& entry) override;
    std::string getSnapshot() override;
    void restoreFromSnapshot(const std::string& snapshot) override;
    
    // 注册中心特有方法
    std::vector<ServiceInstance> getServiceInstances(const std::string& service_name);
    void registerService(const std::string& service_name, const std::string& host, int port);
    void unregisterService(const std::string& service_name, const std::string& host, int port);
    void updateHeartbeat(const std::string& service_name, const std::string& host, int port);
    void removeExpiredServices();
    
    size_t getServiceCount() const;
    std::vector<std::string> getAllServiceNames() const;

private:
    mutable std::mutex services_mutex_;
    std::unordered_map<std::string, std::vector<ServiceInstance>> services_;
    
    void applyServiceOperation(const rpc::ServiceOperation& operation);
};

#endif
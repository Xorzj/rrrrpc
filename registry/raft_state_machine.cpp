#include "raft_state_machine.h"
#include "../common/serializer.h"
#include <iostream>
#include <algorithm>
#include <sstream>

RegistryStateMachine::RegistryStateMachine() {
}

void RegistryStateMachine::applyLogEntry(const rpc::LogEntry& entry) {
    std::cout << "[StateMachine] 应用日志条目: term=" << entry.term() 
              << ", index=" << entry.index() 
              << ", operation=" << entry.operation() << std::endl;
    
    if (entry.operation() == "service_operation") {
        rpc::ServiceOperation operation;
        if (operation.ParseFromString(entry.data())) {
            applyServiceOperation(operation);
        } else {
            std::cerr << "[StateMachine] 解析服务操作失败" << std::endl;
        }
    }
}

void RegistryStateMachine::applyServiceOperation(const rpc::ServiceOperation& operation) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    
    std::cout << "[StateMachine] 执行服务操作: " << operation.operation_type() 
              << " for " << operation.service_name() 
              << " at " << operation.host() << ":" << operation.port() << std::endl;
    
    if (operation.operation_type() == "register") {
        registerService(operation.service_name(), operation.host(), operation.port());
    } else if (operation.operation_type() == "unregister") {
        unregisterService(operation.service_name(), operation.host(), operation.port());
    } else if (operation.operation_type() == "heartbeat") {
        updateHeartbeat(operation.service_name(), operation.host(), operation.port());
    }
}

std::string RegistryStateMachine::getSnapshot() {
    std::lock_guard<std::mutex> lock(services_mutex_);
    
    std::ostringstream oss;
    oss << services_.size() << "\n";
    
    for (const auto& service_pair : services_) {
        const std::string& service_name = service_pair.first;
        const auto& instances = service_pair.second;
        
        oss << service_name << " " << instances.size() << "\n";
        for (const auto& instance : instances) {
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                instance.last_heartbeat.time_since_epoch()).count();
            oss << instance.host << " " << instance.port << " " << timestamp << "\n";
        }
    }
    
    return oss.str();
}

void RegistryStateMachine::restoreFromSnapshot(const std::string& snapshot) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    services_.clear();
    
    std::istringstream iss(snapshot);
    size_t service_count;
    iss >> service_count;
    
    for (size_t i = 0; i < service_count; ++i) {
        std::string service_name;
        size_t instance_count;
        iss >> service_name >> instance_count;
        
        std::vector<ServiceInstance> instances;
        for (size_t j = 0; j < instance_count; ++j) {
            ServiceInstance instance;
            int64_t timestamp;
            iss >> instance.host >> instance.port >> timestamp;
            
            instance.last_heartbeat = std::chrono::steady_clock::time_point(
                std::chrono::milliseconds(timestamp));
            instances.push_back(instance);
        }
        
        services_[service_name] = instances;
    }
    
    std::cout << "[StateMachine] 从快照恢复了 " << service_count << " 个服务" << std::endl;
}

std::vector<ServiceInstance> RegistryStateMachine::getServiceInstances(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    
    auto it = services_.find(service_name);
    if (it != services_.end()) {
        return it->second;
    }
    return {};
}

void RegistryStateMachine::registerService(const std::string& service_name, const std::string& host, int port) {
    // 注意：这个方法在applyServiceOperation中被调用，已经持有锁
    auto& instances = services_[service_name];
    
    // 检查是否已存在
    for (auto& instance : instances) {
        if (instance.host == host && instance.port == port) {
            instance.last_heartbeat = std::chrono::steady_clock::now();
            return;
        }
    }
    
    // 添加新实例
    ServiceInstance instance;
    instance.host = host;
    instance.port = port;
    instance.last_heartbeat = std::chrono::steady_clock::now();
    instances.push_back(instance);
    
    std::cout << "[StateMachine] 注册服务实例: " << service_name 
              << " at " << host << ":" << port << std::endl;
}

void RegistryStateMachine::unregisterService(const std::string& service_name, const std::string& host, int port) {
    // 注意：这个方法在applyServiceOperation中被调用，已经持有锁
    auto it = services_.find(service_name);
    if (it != services_.end()) {
        auto& instances = it->second;
        instances.erase(
            std::remove_if(instances.begin(), instances.end(),
                [&host, port](const ServiceInstance& instance) {
                    return instance.host == host && instance.port == port;
                }),
            instances.end()
        );
        
        if (instances.empty()) {
            services_.erase(it);
        }
        
        std::cout << "[StateMachine] 注销服务实例: " << service_name 
                  << " at " << host << ":" << port << std::endl;
    }
}

void RegistryStateMachine::updateHeartbeat(const std::string& service_name, const std::string& host, int port) {
    // 注意：这个方法在applyServiceOperation中被调用，已经持有锁
    auto it = services_.find(service_name);
    if (it != services_.end()) {
        for (auto& instance : it->second) {
            if (instance.host == host && instance.port == port) {
                instance.last_heartbeat = std::chrono::steady_clock::now();
                return;
            }
        }
    }
}

void RegistryStateMachine::removeExpiredServices() {
    std::lock_guard<std::mutex> lock(services_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(30);
    
    for (auto service_it = services_.begin(); service_it != services_.end();) {
        auto& instances = service_it->second;
        
        instances.erase(
            std::remove_if(instances.begin(), instances.end(),
                [now, timeout](const ServiceInstance& instance) {
                    return (now - instance.last_heartbeat) > timeout;
                }),
            instances.end()
        );
        
        if (instances.empty()) {
            std::cout << "[StateMachine] 移除空服务: " << service_it->first << std::endl;
            service_it = services_.erase(service_it);
        } else {
            ++service_it;
        }
    }
}

size_t RegistryStateMachine::getServiceCount() const {
    std::lock_guard<std::mutex> lock(services_mutex_);
    return services_.size();
}

std::vector<std::string> RegistryStateMachine::getAllServiceNames() const {
    std::lock_guard<std::mutex> lock(services_mutex_);
    
    std::vector<std::string> names;
    for (const auto& pair : services_) {
        names.push_back(pair.first);
    }
    return names;
}
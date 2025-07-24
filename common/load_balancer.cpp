#include "load_balancer.h"
#include <chrono>

LoadBalancer::LoadBalancer(Strategy strategy) 
    : strategy_(strategy), round_robin_index_(0) {
    auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    random_generator_.seed(seed);
}

rpc::ServiceInfo LoadBalancer::selectService(const std::vector<rpc::ServiceInfo>& services) {
    if (services.empty()) {
        return rpc::ServiceInfo();
    }
    
    switch (strategy_) {
        case ROUND_ROBIN:
            {
                size_t index = round_robin_index_ % services.size();
                round_robin_index_++;
                return services[index];
            }
        case RANDOM:
            {
                std::uniform_int_distribution<size_t> dist(0, services.size() - 1);
                size_t index = dist(random_generator_);
                return services[index];
            }
        case LEAST_CONNECTIONS:
            // 简化实现，实际需要维护连接计数
            return services[0];
        default:
            return services[0];
    }
}
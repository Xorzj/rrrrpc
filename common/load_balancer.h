#ifndef LOAD_BALANCER_H
#define LOAD_BALANCER_H

#include "../proto/rpc_message.pb.h"
#include <vector>
#include <random>

class LoadBalancer {
public:
    enum Strategy {
        ROUND_ROBIN,
        RANDOM,
        LEAST_CONNECTIONS
    };
    
    LoadBalancer(Strategy strategy = ROUND_ROBIN);
    
    rpc::ServiceInfo selectService(const std::vector<rpc::ServiceInfo>& services);
    
private:
    Strategy strategy_;
    size_t round_robin_index_;
    std::mt19937 random_generator_;
};

#endif
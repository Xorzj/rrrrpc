#include "../client/rpc_client.h"
#include "../common/network.h"
#include "../common/serializer.h"
#include "../proto/rpc_message.pb.h"
#include <iostream>

int main() {
    std::cout << "=== Service Discovery Test ===" << std::endl;
    
    try {
        // 直接测试服务发现
        TcpClient client;
        std::cout << "1. Connecting to registry..." << std::endl;
        
        if (!client.connect("127.0.0.1", 8080)) {
            std::cout << "✗ Failed to connect to registry" << std::endl;
            return 1;
        }
        std::cout << "✓ Connected to registry" << std::endl;
        
        // 测试EchoService发现
        std::cout << "2. Testing EchoService discovery..." << std::endl;
        rpc::ServiceDiscoveryRequest request;
        request.set_service_name("EchoService");
        
        std::string request_data = Serializer::serialize(request);
        std::cout << "   Request size: " << request_data.size() << " bytes" << std::endl;
        
        if (!client.send(request_data)) {
            std::cout << "✗ Failed to send request" << std::endl;
            return 1;
        }
        std::cout << "✓ Request sent" << std::endl;
        
        std::string response_data = client.receive();
        std::cout << "   Response size: " << response_data.size() << " bytes" << std::endl;
        
        if (response_data.empty()) {
            std::cout << "✗ Empty response" << std::endl;
            return 1;
        }
        
        rpc::ServiceDiscoveryResponse response;
        if (!Serializer::deserialize(response_data, &response)) {
            std::cout << "✗ Failed to parse response" << std::endl;
            return 1;
        }
        
        std::cout << "✓ Response parsed, services count: " << response.services_size() << std::endl;
        
        for (int i = 0; i < response.services_size(); i++) {
            const auto& service = response.services(i);
            std::cout << "   Service " << i << ": " << service.host() << ":" << service.port() << std::endl;
        }
        
        client.disconnect();
        std::cout << "=== Test Completed ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "✗ Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
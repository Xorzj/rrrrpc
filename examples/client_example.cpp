#include "../client/rpc_client.h"
#include "../proto/service.pb.h"
#include "../common/serializer.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <thread>

std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

int main() {
    std::cout << "=== RPC Client Test ===" << std::endl;
    std::cout << "[" << getCurrentTime() << "] Starting RPC client..." << std::endl;
    
    try {
        // 创建RPC客户端，连接到8080端口的注册中心
        RpcClient client("127.0.0.1", 8080);
        std::cout << "[" << getCurrentTime() << "] Connected to registry at 127.0.0.1:8080" << std::endl;
        
        // 等待服务注册完成
        std::cout << "[" << getCurrentTime() << "] Waiting for services to be available..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        std::cout << "===========================================" << std::endl;
        
        // 测试1: EchoService
        std::cout << "\n[TEST 1] Testing EchoService..." << std::endl;
        {
            example::EchoRequest request;
            request.set_message("Hello, RPC Framework!");
            
            std::string request_data = Serializer::serialize(request);
            std::cout << "[CLIENT] Sending Echo request: \"" << request.message() << "\"" << std::endl;
            
            std::string response_data = client.call("EchoService", "echo", request_data);
            
            if (!response_data.empty()) {
                example::EchoResponse response;
                if (Serializer::deserialize(response_data, &response)) {
                    std::cout << "[CLIENT] ✓ Echo response: \"" << response.message() << "\"" << std::endl;
                } else {
                    std::cout << "[CLIENT] ✗ Failed to deserialize Echo response" << std::endl;
                }
            } else {
                std::cout << "[CLIENT] ✗ Empty response from Echo service" << std::endl;
            }
        }
        
        // 测试2: CalculatorService
        std::cout << "\n[TEST 2] Testing CalculatorService..." << std::endl;
        {
            example::AddRequest request;
            request.set_a(15);
            request.set_b(25);
            
            std::string request_data = Serializer::serialize(request);
            std::cout << "[CLIENT] Sending Add request: " << request.a() << " + " << request.b() << std::endl;
            
            std::string response_data = client.call("CalculatorService", "add", request_data);
            
            if (!response_data.empty()) {
                example::AddResponse response;
                if (Serializer::deserialize(response_data, &response)) {
                    std::cout << "[CLIENT] ✓ Add result: " << request.a() << " + " << request.b() 
                             << " = " << response.result() << std::endl;
                } else {
                    std::cout << "[CLIENT] ✗ Failed to deserialize Add response" << std::endl;
                }
            } else {
                std::cout << "[CLIENT] ✗ Empty response from Calculator service" << std::endl;
            }
        }
        
        // 测试3: 多次请求测试
        std::cout << "\n[TEST 3] Multiple requests test..." << std::endl;
        for (int i = 1; i <= 3; i++) {
            example::AddRequest request;
            request.set_a(i * 10);
            request.set_b(i * 5);
            
            std::string request_data = Serializer::serialize(request);
            std::cout << "[CLIENT] Request " << i << ": " << request.a() << " + " << request.b() << std::endl;
            
            std::string response_data = client.call("CalculatorService", "add", request_data);
            
            if (!response_data.empty()) {
                example::AddResponse response;
                if (Serializer::deserialize(response_data, &response)) {
                    std::cout << "[CLIENT] ✓ Result " << i << ": " << response.result() << std::endl;
                } else {
                    std::cout << "[CLIENT] ✗ Failed to deserialize response " << i << std::endl;
                }
            } else {
                std::cout << "[CLIENT] ✗ Empty response for request " << i << std::endl;
            }
            
            // 短暂延迟
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // 测试4: 错误服务测试
        std::cout << "\n[TEST 4] Testing non-existent service..." << std::endl;
        {
            example::EchoRequest request;
            request.set_message("Test non-existent service");
            
            std::string request_data = Serializer::serialize(request);
            std::cout << "[CLIENT] Calling non-existent service..." << std::endl;
            
            std::string response_data = client.call("NonExistentService", "test", request_data);
            
            if (response_data.empty()) {
                std::cout << "[CLIENT] ✓ Correctly failed to find non-existent service" << std::endl;
            } else {
                std::cout << "[CLIENT] ✗ Unexpected response for non-existent service" << std::endl;
            }
        }
        
        // 测试5: 连续Echo测试
        std::cout << "\n[TEST 5] Continuous Echo test..." << std::endl;
        std::vector<std::string> test_messages = {
            "Hello World!",
            "RPC Framework Test",
            "多字节测试 UTF-8",
            "Special chars: !@#$%^&*()",
            "Numbers: 12345"
        };
        
        for (size_t i = 0; i < test_messages.size(); i++) {
            example::EchoRequest request;
            request.set_message(test_messages[i]);
            
            std::string request_data = Serializer::serialize(request);
            std::cout << "[CLIENT] Echo test " << (i+1) << ": \"" << request.message() << "\"" << std::endl;
            
            std::string response_data = client.call("EchoService", "echo", request_data);
            
            if (!response_data.empty()) {
                example::EchoResponse response;
                if (Serializer::deserialize(response_data, &response)) {
                    std::cout << "[CLIENT] ✓ Response: \"" << response.message() << "\"" << std::endl;
                } else {
                    std::cout << "[CLIENT] ✗ Failed to deserialize response" << std::endl;
                }
            } else {
                std::cout << "[CLIENT] ✗ Empty response" << std::endl;
            }
        }
        
        std::cout << "\n===========================================" << std::endl;
        std::cout << "[" << getCurrentTime() << "] All tests completed!" << std::endl;
        std::cout << "RPC Framework is working correctly! 🎉" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] RPC client error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
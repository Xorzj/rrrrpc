#include "../server/rpc_server.h"
#include "../proto/service.pb.h"
#include "../common/serializer.h"
#include <iostream>

std::string echoHandler(const std::string& request_data) {
    example::EchoRequest request;
    if (!Serializer::deserialize(request_data, &request)) {
        throw std::runtime_error("Failed to deserialize request");
    }
    
    example::EchoResponse response;
    response.set_message("Echo: " + request.message());
    
    return Serializer::serialize(response);
}

std::string addHandler(const std::string& request_data) {
    example::AddRequest request;
    if (!Serializer::deserialize(request_data, &request)) {
        throw std::runtime_error("Failed to deserialize request");
    }
    
    example::AddResponse response;
    response.set_result(request.a() + request.b());
    
    return Serializer::serialize(response);
}

int main() {
    // 创建RPC服务器
    RpcServer server(8081, "127.0.0.1", 8080);
    
    // 注册服务方法
    server.registerService("EchoService", "echo", echoHandler);
    server.registerService("CalculatorService", "add", addHandler);
    
    std::cout << "Starting RPC server on port 8081..." << std::endl;
    
    // 启动服务器
    server.start();
    
    return 0;
}
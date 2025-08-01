cmake_minimum_required(VERSION 3.10)
project(rrrrpc)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖包
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${Protobuf_INCLUDE_DIRS})

# 生成protobuf文件
set(PROTO_FILES
    proto/rpc_message.proto
    proto/service.proto
    proto/distributed_registry.proto
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# 公共库源文件
set(COMMON_SOURCES
    common/network.cpp
    common/serializer.cpp
    common/thread_pool.cpp
    common/connection_pool.cpp
    common/epoll_server.cpp
    common/load_balancer.cpp
)

# 注册中心源文件
set(REGISTRY_SOURCES
    registry/registry_server.cpp
    registry/raft_state_machine.cpp
    registry/raft_node.cpp
    registry/raft_node_rpc.cpp
    registry/distributed_registry_server.cpp
)

# 服务器源文件
set(SERVER_SOURCES
    server/rpc_server.cpp
)

# 客户端源文件
set(CLIENT_SOURCES
    client/rpc_client.cpp
)

# 创建公共库
add_library(rpc_common STATIC
    ${COMMON_SOURCES}
    ${PROTO_SRCS}
)

target_link_libraries(rpc_common
    ${Protobuf_LIBRARIES}
    Threads::Threads
)

# 创建注册中心库
add_library(rpc_registry STATIC
    ${REGISTRY_SOURCES}
)

target_link_libraries(rpc_registry
    rpc_common
    ${Protobuf_LIBRARIES}
    Threads::Threads
)

# 创建服务器库
add_library(rpc_server STATIC
    ${SERVER_SOURCES}
)

target_link_libraries(rpc_server
    rpc_common
    ${Protobuf_LIBRARIES}
    Threads::Threads
)

# 创建客户端库
add_library(rpc_client STATIC
    ${CLIENT_SOURCES}
)

target_link_libraries(rpc_client
    rpc_common
    ${Protobuf_LIBRARIES}
    Threads::Threads
)

# 编译示例程序

# 原始注册中心示例
add_executable(registry_example
    examples/registry_example.cpp
)

target_link_libraries(registry_example
    rpc_registry
    rpc_common
    ${Protobuf_LIBRARIES}
    Threads::Threads
)

# 分布式注册中心示例
add_executable(distributed_registry_example
    examples/distributed_registry_example.cpp
)

target_link_libraries(distributed_registry_example
    rpc_registry
    rpc_common
    ${Protobuf_LIBRARIES}
    Threads::Threads
)

# 服务器示例
add_executable(server_example
    examples/server_example.cpp
)

target_link_libraries(server_example
    rpc_server
    rpc_common
    ${Protobuf_LIBRARIES}
    Threads::Threads
)

# 客户端示例
add_executable(client_example
    examples/client_example.cpp
)

target_link_libraries(client_example
    rpc_client
    rpc_common
    ${Protobuf_LIBRARIES}
    Threads::Threads
)

# 测试程序
add_executable(simple_registry_test
    test/simple_registry_test.cpp
)

target_link_libraries(simple_registry_test
    rpc_registry
    rpc_common
    ${Protobuf_LIBRARIES}
    Threads::Threads
)

add_executable(test_discovery
    test/test_discovery.cpp
)

target_link_libraries(test_discovery
    rpc_client
    rpc_common
    ${Protobuf_LIBRARIES}
    Threads::Threads
)

# 设置输出目录
set_target_properties(
    registry_example
    distributed_registry_example
    server_example
    client_example
    simple_registry_test
    test_discovery
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 编译选项
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(rpc_common PRIVATE -Wall -Wextra -O2)
    target_compile_options(rpc_registry PRIVATE -Wall -Wextra -O2)
    target_compile_options(rpc_server PRIVATE -Wall -Wextra -O2)
    target_compile_options(rpc_client PRIVATE -Wall -Wextra -O2)
endif()

# 安装规则
install(TARGETS rpc_common rpc_registry rpc_server rpc_client
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY common/ registry/ server/ client/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)
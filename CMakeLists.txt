cmake_minimum_required(VERSION 3.10)
project(rpc_framework)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

# 设置protobuf相关
set(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/proto")

# 生成protobuf文件到proto目录
set(PROTO_FILES
    ${PROTO_PATH}/rpc_message.proto
    ${PROTO_PATH}/service.proto
)

set(GENERATED_PROTOBUF_FILES "")
foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    # 直接生成到proto目录
    set(GENERATED_PB_CC "${PROTO_PATH}/${PROTO_NAME}.pb.cc")
    set(GENERATED_PB_H "${PROTO_PATH}/${PROTO_NAME}.pb.h")
    
    list(APPEND GENERATED_PROTOBUF_FILES ${GENERATED_PB_CC} ${GENERATED_PB_H})
    
    add_custom_command(
        OUTPUT ${GENERATED_PB_CC} ${GENERATED_PB_H}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --cpp_out=${PROTO_PATH} --proto_path=${PROTO_PATH} ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating ${PROTO_NAME} in proto directory"
        VERBATIM
    )
endforeach()

# 添加自定义目标来确保protobuf文件先生成
add_custom_target(generate_protos DEPENDS ${GENERATED_PROTOBUF_FILES})

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${PROTO_PATH})
include_directories(${Protobuf_INCLUDE_DIRS})

# 检查源文件是否存在，如果不存在则创建空文件
set(REQUIRED_SOURCES
    common/network.cpp
    common/serializer.cpp
    common/thread_pool.cpp
    common/epoll_server.cpp
    common/connection_pool.cpp
    registry/registry_server.cpp
    server/rpc_server.cpp
    client/rpc_client.cpp
)

foreach(SOURCE_FILE ${REQUIRED_SOURCES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}")
        message(WARNING "Source file ${SOURCE_FILE} does not exist, creating placeholder")
        file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}" "// Placeholder file\n")
    endif()
endforeach()

# 库源文件 - 只包含存在的文件
set(LIB_SOURCES "")

# 检查并添加源文件
foreach(SOURCE_FILE ${REQUIRED_SOURCES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}")
        list(APPEND LIB_SOURCES ${SOURCE_FILE})
        message(STATUS "Added source: ${SOURCE_FILE}")
    else()
        message(WARNING "Missing source file: ${SOURCE_FILE}")
    endif()
endforeach()

# 添加生成的protobuf文件
list(APPEND LIB_SOURCES ${GENERATED_PROTOBUF_FILES})

# 打印调试信息
message(STATUS "LIB_SOURCES contains ${LIB_SOURCES}")

# 创建静态库
if(LIB_SOURCES)
    add_library(rpc_framework STATIC ${LIB_SOURCES})
    add_dependencies(rpc_framework generate_protos)
    target_link_libraries(rpc_framework ${Protobuf_LIBRARIES} Threads::Threads)
else()
    message(FATAL_ERROR "No source files found for rpc_framework library")
endif()

# 检查示例文件是否存在
set(EXAMPLE_FILES
    examples/registry_example.cpp
    examples/server_example.cpp
    examples/client_example.cpp
)

foreach(EXAMPLE_FILE ${EXAMPLE_FILES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_FILE}")
        message(WARNING "Example file ${EXAMPLE_FILE} does not exist, creating placeholder")
        get_filename_component(EXAMPLE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_FILE}" DIRECTORY)
        file(MAKE_DIRECTORY "${EXAMPLE_DIR}")
        file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_FILE}" 
            "// Placeholder example file\n#include <iostream>\nint main() { std::cout << \"Placeholder\" << std::endl; return 0; }\n")
    endif()
endforeach()

# 示例程序
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/registry_example.cpp")
    add_executable(registry_server examples/registry_example.cpp)
    target_link_libraries(registry_server rpc_framework)
    add_dependencies(registry_server generate_protos)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/server_example.cpp")
    add_executable(rpc_server examples/server_example.cpp)
    target_link_libraries(rpc_server rpc_framework)
    add_dependencies(rpc_server generate_protos)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/client_example.cpp")
    add_executable(rpc_client examples/client_example.cpp)
    target_link_libraries(rpc_client rpc_framework)
    add_dependencies(rpc_client generate_protos)
endif()

# 测试程序（如果存在）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/simple_registry_test.cpp")
    add_executable(simple_registry_test test/simple_registry_test.cpp)
    target_link_libraries(simple_registry_test rpc_framework)
    add_dependencies(simple_registry_test generate_protos)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/test_discovery.cpp")
    add_executable(test_discovery test/test_discovery.cpp)
    target_link_libraries(test_discovery rpc_framework)
    add_dependencies(test_discovery generate_protos)
endif()

# 清理规则
set_property(DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    "${PROTO_PATH}/rpc_message.pb.cc"
    "${PROTO_PATH}/rpc_message.pb.h"
    "${PROTO_PATH}/service.pb.cc" 
    "${PROTO_PATH}/service.pb.h"
)

# 安装规则
install(TARGETS rpc_framework DESTINATION lib)
install(DIRECTORY common/ DESTINATION include/rpc_framework/common
        FILES_MATCHING PATTERN "*.h")
install(DIRECTORY registry/ DESTINATION include/rpc_framework/registry
        FILES_MATCHING PATTERN "*.h")
install(DIRECTORY server/ DESTINATION include/rpc_framework/server
        FILES_MATCHING PATTERN "*.h")
install(DIRECTORY client/ DESTINATION include/rpc_framework/client
        FILES_MATCHING PATTERN "*.h")
install(DIRECTORY proto/ DESTINATION include/rpc_framework/proto
        FILES_MATCHING PATTERN "*.h")
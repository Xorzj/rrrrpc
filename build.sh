#!/bin/bash

# 创建构建目录
mkdir -p build
cd build

# 配置和构建
cmake ..
make -j$(nproc)

echo "Build completed successfully!"
echo "Executables:"
echo "  - registry_server: Registry server"
echo "  - rpc_server: RPC service provider"
echo "  - rpc_client: RPC client example"
#!/bin/bash

set -e

echo "=== RPC Framework Demo ==="
echo "This script will demonstrate the RPC framework working"
echo ""

# 确保在正确的目录
cd "$(dirname "$0")/.."

# 检查构建
if [ ! -d "build" ]; then
    echo "Build directory not found. Running build first..."
    ./build.sh
fi

cd build

# 检查可执行文件
for exe in registry_server rpc_server rpc_client; do
    if [ ! -f "$exe" ]; then
        echo "Error: $exe not found. Please run build.sh first."
        exit 1
    fi
done

echo "Starting RPC Framework Demo..."
echo "You will see output from 3 components:"
echo "1. Registry Server (service discovery)"
echo "2. RPC Server (service provider)" 
echo "3. RPC Client (service consumer)"
echo ""
echo "Press Ctrl+C to stop all services when done."
echo ""

# 创建日志目录
mkdir -p logs

# 启动注册中心
echo "Step 1: Starting Registry Server..."
./registry_server > logs/registry.log 2>&1 &
REGISTRY_PID=$!
sleep 2

# 检查注册中心是否启动成功
if ! kill -0 $REGISTRY_PID 2>/dev/null; then
    echo "Error: Registry server failed to start"
    cat logs/registry.log
    exit 1
fi
echo "✓ Registry server started (PID: $REGISTRY_PID)"

# 启动RPC服务器
echo "Step 2: Starting RPC Server..."
./rpc_server > logs/server.log 2>&1 &
SERVER_PID=$!
sleep 3

# 检查RPC服务器是否启动成功
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "Error: RPC server failed to start"
    cat logs/server.log
    kill $REGISTRY_PID 2>/dev/null
    exit 1
fi
echo "✓ RPC server started (PID: $SERVER_PID)"

# 运行客户端测试
echo "Step 3: Running RPC Client tests..."
echo ""
echo "=== CLIENT OUTPUT ==="
./rpc_client

echo ""
echo "=== REGISTRY SERVER LOG ==="
cat logs/registry.log | tail -10

echo ""
echo "=== RPC SERVER LOG ==="  
cat logs/server.log | tail -10

echo ""
echo "=== Demo completed successfully! ==="

# 清理进程
echo "Cleaning up..."
kill $SERVER_PID 2>/dev/null || true
kill $REGISTRY_PID 2>/dev/null || true

echo "All processes stopped."
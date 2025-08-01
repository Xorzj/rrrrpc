#!/bin/bash

# 分布式注册中心启动脚本

echo "正在构建分布式注册中心..."

# 创建构建目录
mkdir -p build
cd build

# 编译项目
cmake ..
make -j4

if [ $? -ne 0 ]; then
    echo "编译失败！"
    exit 1
fi

echo "编译成功！"

# 返回项目根目录
cd ..

# 创建日志目录
mkdir -p logs

echo "启动分布式注册中心集群..."

# 启动三个注册中心节点
echo "启动节点1 (端口: 8080/9080)..."
./build/bin/distributed_registry_example 0 > logs/registry_node_1.log 2>&1 &
NODE1_PID=$!

sleep 2

echo "启动节点2 (端口: 8081/9081)..."
./build/bin/distributed_registry_example 1 > logs/registry_node_2.log 2>&1 &
NODE2_PID=$!

sleep 2

echo "启动节点3 (端口: 8082/9082)..."
./build/bin/distributed_registry_example 2 > logs/registry_node_3.log 2>&1 &
NODE3_PID=$!

echo "分布式注册中心集群启动完成！"
echo "节点1 PID: $NODE1_PID (客户端端口: 8080)"
echo "节点2 PID: $NODE2_PID (客户端端口: 8081)"
echo "节点3 PID: $NODE3_PID (客户端端口: 8082)"
echo ""
echo "日志文件位置:"
echo "  节点1: logs/registry_node_1.log"
echo "  节点2: logs/registry_node_2.log"
echo "  节点3: logs/registry_node_3.log"
echo ""
echo "要停止集群，请运行: ./scripts/stop_distributed_registry.sh"

# 保存PID到文件
echo "$NODE1_PID" > logs/node1.pid
echo "$NODE2_PID" > logs/node2.pid
echo "$NODE3_PID" > logs/node3.pid

# 等待用户输入
echo "按 Ctrl+C 停止所有节点..."
trap 'kill $NODE1_PID $NODE2_PID $NODE3_PID; exit' INT

# 监控进程
while true; do
    if ! kill -0 $NODE1_PID 2>/dev/null; then
        echo "节点1已停止"
        break
    fi
    if ! kill -0 $NODE2_PID 2>/dev/null; then
        echo "节点2已停止"
        break
    fi
    if ! kill -0 $NODE3_PID 2>/dev/null; then
        echo "节点3已停止"
        break
    fi
    sleep 5
done

echo "分布式注册中心集群已停止"
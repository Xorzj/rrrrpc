#!/bin/bash

# 分布式注册中心停止脚本

echo "正在停止分布式注册中心集群..."

# 检查PID文件是否存在
if [ -f "logs/node1.pid" ]; then
    NODE1_PID=$(cat logs/node1.pid)
    if kill -0 $NODE1_PID 2>/dev/null; then
        echo "停止节点1 (PID: $NODE1_PID)..."
        kill $NODE1_PID
    else
        echo "节点1已经停止"
    fi
    rm -f logs/node1.pid
fi

if [ -f "logs/node2.pid" ]; then
    NODE2_PID=$(cat logs/node2.pid)
    if kill -0 $NODE2_PID 2>/dev/null; then
        echo "停止节点2 (PID: $NODE2_PID)..."
        kill $NODE2_PID
    else
        echo "节点2已经停止"
    fi
    rm -f logs/node2.pid
fi

if [ -f "logs/node3.pid" ]; then
    NODE3_PID=$(cat logs/node3.pid)
    if kill -0 $NODE3_PID 2>/dev/null; then
        echo "停止节点3 (PID: $NODE3_PID)..."
        kill $NODE3_PID
    else
        echo "节点3已经停止"
    fi
    rm -f logs/node3.pid
fi

# 等待进程完全停止
sleep 2

# 强制杀死任何剩余的进程
pkill -f "distributed_registry_example" 2>/dev/null

echo "分布式注册中心集群已停止"
echo "日志文件仍保留在 logs/ 目录中"
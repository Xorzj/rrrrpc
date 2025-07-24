# RPC Framework

一个基于C++和Protocol Buffers的简单RPC框架，支持服务注册发现、负载均衡和心跳检测。

## 特性

- 基于TCP的网络通信
- Protocol Buffers序列化
- 服务注册与发现
- 心跳检测机制
- 负载均衡支持
- 连接池管理
- 异步处理能力

## 架构组件

### 注册中心 (Registry Server)
- 服务注册和注销
- 服务发现
- 健康检查和心跳监控
- 服务实例管理

### RPC服务端 (RPC Server)
- 服务提供者
- 自动服务注册
- 定期心跳发送
- 多线程请求处理

### RPC客户端 (RPC Client)
- 服务消费者
- 自动服务发现
- 负载均衡
- 连接池管理

## 快速开始

### 1. 安装依赖

```bash
# Ubuntu/Debian
sudo apt-get install libprotobuf-dev protobuf-compiler cmake build-essential

# CentOS/RHEL
sudo yum install protobuf-devel protobuf-compiler cmake gcc-c++
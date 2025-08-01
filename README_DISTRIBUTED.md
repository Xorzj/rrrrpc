# 分布式注册中心

这是一个基于Raft一致性算法实现的分布式RPC注册中心，提供高可用性和数据一致性保证。

## 架构特性

### 核心组件
- **Raft一致性算法**: 确保集群数据一致性和Leader选举
- **分布式状态机**: 管理服务注册信息的复制和同步
- **故障转移**: 自动Leader选举和故障恢复
- **数据持久化**: 日志复制确保数据不丢失

### 主要功能
- ✅ 服务注册与发现
- ✅ 心跳监控和自动清理
- ✅ 集群状态查询
- ✅ Leader选举和故障转移
- ✅ 数据一致性保证
- ✅ 水平扩展支持

## 快速开始

### 1. 编译项目

```bash
# 创建构建目录
mkdir build && cd build

# 配置和编译
cmake ..
make -j4
```

### 2. 启动分布式集群

使用提供的脚本启动3节点集群：

```bash
# 给脚本执行权限
chmod +x scripts/start_distributed_registry.sh
chmod +x scripts/stop_distributed_registry.sh

# 启动集群
./scripts/start_distributed_registry.sh
```

这将启动3个注册中心节点：
- 节点1: 客户端端口8080, Raft端口9080
- 节点2: 客户端端口8081, Raft端口9081  
- 节点3: 客户端端口8082, Raft端口9082

### 3. 手动启动单个节点

```bash
# 启动节点1
./build/bin/distributed_registry_example 0

# 启动节点2
./build/bin/distributed_registry_example 1

# 启动节点3
./build/bin/distributed_registry_example 2
```

### 4. 测试集群功能

```bash
# 编译测试程序
cd build && make test_distributed_registry

# 运行测试
./bin/test_distributed_registry
```

## 集群配置

### 配置文件 (config/distributed_registry_config.json)

```json
{
    "cluster": {
        "nodes": [
            {
                "node_id": "registry-node-1",
                "host": "127.0.0.1",
                "client_port": 8080,
                "raft_port": 9080
            },
            {
                "node_id": "registry-node-2", 
                "host": "127.0.0.1",
                "client_port": 8081,
                "raft_port": 9081
            },
            {
                "node_id": "registry-node-3",
                "host": "127.0.0.1", 
                "client_port": 8082,
                "raft_port": 9082
            }
        ]
    },
    "raft": {
        "election_timeout_min": 150,
        "election_timeout_max": 300,
        "heartbeat_interval": 50
    }
}
```

## API接口

### 服务注册

```cpp
rpc::ServiceRegisterRequest request;
request.set_service_name("my-service");
request.set_host("127.0.0.1");
request.set_port(8080);

// 发送到任意节点，会自动转发到Leader
```

### 服务发现

```cpp
rpc::ServiceDiscoveryRequest request;
request.set_service_name("my-service");

// 可以从任意节点查询
```

### 心跳维持

```cpp
rpc::HeartbeatRequest request;
request.set_service_name("my-service");
request.set_host("127.0.0.1");
request.set_port(8080);

// 定期发送心跳到Leader节点
```

### 集群状态查询

```cpp
rpc::ClusterStatusRequest request;

// 查询集群状态和Leader信息
```

## 故障处理

### Leader故障
- 自动检测Leader失效
- 触发新的Leader选举
- 客户端自动重连新Leader

### 节点故障
- 故障节点自动从集群中移除
- 数据在剩余节点间保持一致
- 故障节点恢复后自动同步数据

### 网络分区
- 多数派继续提供服务
- 少数派进入只读模式
- 网络恢复后自动同步

## 性能特性

### 一致性保证
- **强一致性**: 所有写操作通过Raft协议保证一致性
- **线性化**: 读写操作具有线性化语义
- **持久性**: 数据写入多数节点后才确认成功

### 可用性
- **高可用**: 支持n/2-1个节点故障 (n为总节点数)
- **自动恢复**: 故障节点重启后自动加入集群
- **无单点故障**: 任意节点都可以成为Leader

### 扩展性
- **水平扩展**: 支持动态添加节点
- **负载均衡**: 读请求可以分散到多个节点
- **存储优化**: 支持日志压缩和快照

## 监控和运维

### 日志文件
```
logs/
├── registry_node_1.log  # 节点1日志
├── registry_node_2.log  # 节点2日志
└── registry_node_3.log  # 节点3日志
```

### 关键指标
- Leader选举次数
- 日志复制延迟
- 服务注册数量
- 心跳超时次数

### 运维命令

```bash
# 停止集群
./scripts/stop_distributed_registry.sh

# 查看节点状态
tail -f logs/registry_node_1.log

# 检查进程
ps aux | grep distributed_registry_example
```

## 故障排查

### 常见问题

1. **节点无法启动**
   - 检查端口是否被占用
   - 确认配置文件格式正确
   - 查看日志文件错误信息

2. **Leader选举失败**
   - 确保至少有2个节点运行
   - 检查网络连接
   - 验证Raft端口通信

3. **服务注册失败**
   - 确认连接的是Leader节点
   - 检查网络连接状态
   - 验证请求格式正确

### 调试模式

编译时启用调试信息：
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j4
```

## 与原版注册中心对比

| 特性 | 原版注册中心 | 分布式注册中心 |
|------|-------------|----------------|
| 可用性 | 单点故障 | 高可用 |
| 数据一致性 | 单机一致 | 分布式强一致 |
| 扩展性 | 垂直扩展 | 水平扩展 |
| 故障恢复 | 手动重启 | 自动故障转移 |
| 部署复杂度 | 简单 | 中等 |

## 生产环境建议

1. **节点数量**: 建议使用3或5个节点
2. **网络**: 确保节点间网络延迟低于10ms
3. **存储**: 使用SSD存储提高日志写入性能
4. **监控**: 部署监控系统跟踪集群状态
5. **备份**: 定期备份集群数据和配置

## 开发和贡献

### 代码结构
```
registry/
├── raft_state_machine.h/cpp     # Raft状态机
├── raft_node.h/cpp              # Raft节点实现
├── raft_node_rpc.cpp            # Raft RPC处理
├── distributed_registry_server.h/cpp  # 分布式注册中心服务
└── registry_server.h/cpp        # 原版注册中心(兼容)
```

### 扩展功能
- 支持更多一致性级别
- 实现动态配置变更
- 添加性能监控指标
- 支持数据压缩和加密

欢迎提交Issue和Pull Request！
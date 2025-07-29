#include "connection_pool.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <iostream>

// 默认构造函数
ConnectionPool::ConnectionPool() : ConnectionPool(Config{}) {}

// 带配置的构造函数
ConnectionPool::ConnectionPool(const Config& config)
    : config_(config), running_(true) {
  std::cout << "[ConnectionPool] Creating connection pool" << std::endl;
  std::cout << "[ConnectionPool] Max connections per host: "
            << config_.max_connections_per_host << std::endl;
  std::cout << "[ConnectionPool] Min connections per host: "
            << config_.min_connections_per_host << std::endl;
  std::cout << "[ConnectionPool] Connection timeout: "
            << config_.connection_timeout.count() << "s" << std::endl;
  std::cout << "[ConnectionPool] Idle timeout: " << config_.idle_timeout.count()
            << "s" << std::endl;
  std::cout << "[ConnectionPool] Acquire timeout: "
            << config_.acquire_timeout.count() << "s" << std::endl;

  // 启动清理线程
  cleanup_thread_ = std::thread([this]() { runCleanupLoop(); });
}

ConnectionPool::~ConnectionPool() { stop(); }

void ConnectionPool::stop() {
  if (!running_) return;

  std::cout << "[ConnectionPool] Stopping connection pool..." << std::endl;
  running_ = false;

  // 停止清理线程
  if (cleanup_thread_.joinable()) {
    cleanup_thread_.join();
  }

  // 清理所有连接池
  std::unique_lock<std::shared_mutex> lock(pools_mutex_);
  for (auto& pool_pair : pools_) {
    HostPool* pool = pool_pair.second.get();
    std::unique_lock<std::mutex> pool_lock(pool->mutex);

    // 关闭所有连接
    for (auto& pooled_conn : pool->all_connections) {
      if (pooled_conn->connection && pooled_conn->connection->isConnected()) {
        pooled_conn->connection->close();
      }
    }

    pool->all_connections.clear();

    // 清空空闲队列
    while (!pool->idle_connections.empty()) {
      pool->idle_connections.pop();
    }
  }
  pools_.clear();

  std::cout << "[ConnectionPool] Connection pool stopped" << std::endl;
}

std::shared_ptr<TcpConnection> ConnectionPool::acquire(const std::string& host,
                                                       int port) {
  if (!running_) {
    throw std::runtime_error("Connection pool is stopped");
  }

  std::string key = makeKey(host, port);
  HostPool* pool = getOrCreateHostPool(key);

  std::unique_lock<std::mutex> lock(pool->mutex);
  auto timeout_time =
      std::chrono::steady_clock::now() + config_.acquire_timeout;

  while (running_) {
    // 1. 尝试从空闲连接中获取
    while (!pool->idle_connections.empty()) {
      auto pooled_conn = pool->idle_connections.front();
      pool->idle_connections.pop();

      // 检查连接是否仍然有效
      if (isConnectionValid(pooled_conn->connection)) {
        pooled_conn->in_use = true;
        pooled_conn->last_used = std::chrono::steady_clock::now();
        pool->active_count++;

        std::cout << "[ConnectionPool] ✓ Reused connection to " << host << ":"
                  << port << " (active: " << pool->active_count << "/"
                  << pool->getTotalConnections() << ")" << std::endl;

        return pooled_conn->connection;
      } else {
        // 移除无效连接
        removeConnection(pool, pooled_conn);
        std::cout << "[ConnectionPool] Removed invalid connection to " << host
                  << ":" << port << std::endl;
      }
    }

    // 2. 如果没有空闲连接，尝试创建新连接
    if (pool->getTotalConnections() < config_.max_connections_per_host) {
      lock.unlock();

      auto new_conn = createConnection(host, port);
      if (new_conn) {
        lock.lock();

        auto pooled_conn = std::make_shared<PooledConnection>(new_conn);
        pooled_conn->in_use = true;
        pool->all_connections.push_back(pooled_conn);
        pool->active_count++;

        std::cout << "[ConnectionPool] ✓ Created new connection to " << host
                  << ":" << port << " (active: " << pool->active_count << "/"
                  << pool->getTotalConnections() << ")" << std::endl;

        return new_conn;
      } else {
        lock.lock();
        std::cout << "[ConnectionPool] ✗ Failed to create new connection to "
                  << host << ":" << port << std::endl;
      }
    }

    // 3. 等待连接可用或超时
    if (std::chrono::steady_clock::now() >= timeout_time) {
      throw std::runtime_error("Failed to acquire connection to " + host + ":" +
                               std::to_string(port) + " - timeout");
    }

    std::cout << "[ConnectionPool] Waiting for available connection to " << host
              << ":" << port << " (active: " << pool->active_count << "/"
              << pool->getTotalConnections() << ")" << std::endl;

    auto wait_until =
        std::min(timeout_time, std::chrono::steady_clock::now() +
                                   std::chrono::milliseconds(100));
    pool->condition.wait_until(lock, wait_until);
  }

  throw std::runtime_error("Connection pool is shutting down");
}

// 其余方法保持不变...
// (这里为了节省空间，我不重复所有方法，它们和之前提供的实现相同)

void ConnectionPool::release(const std::string& host, int port,
                             std::shared_ptr<TcpConnection> conn) {
  if (!running_ || !conn) return;

  std::string key = makeKey(host, port);

  std::shared_lock<std::shared_mutex> shared_lock(pools_mutex_);
  auto pool_it = pools_.find(key);
  if (pool_it == pools_.end()) {
    std::cout << "[ConnectionPool] ⚠ Pool not found for " << host << ":" << port
              << ", closing connection" << std::endl;
    conn->close();
    return;
  }

  HostPool* pool = pool_it->second.get();
  shared_lock.unlock();

  std::unique_lock<std::mutex> lock(pool->mutex);

  // 找到对应的连接
  auto it =
      std::find_if(pool->all_connections.begin(), pool->all_connections.end(),
                   [&](const std::shared_ptr<PooledConnection>& pooled_conn) {
                     return pooled_conn->connection.get() == conn.get();
                   });

  if (it != pool->all_connections.end()) {
    auto pooled_conn = *it;

    if (pooled_conn->in_use) {
      pooled_conn->in_use = false;
      pooled_conn->last_used = std::chrono::steady_clock::now();
      pool->active_count--;

      // 检查连接是否仍然有效
      if (isConnectionValid(conn)) {
        pool->idle_connections.push(pooled_conn);

        std::cout << "[ConnectionPool] ✓ Released connection to " << host << ":"
                  << port << " (active: " << pool->active_count
                  << ", idle: " << pool->getIdleConnections() << ")"
                  << std::endl;

        // 通知等待的线程
        pool->condition.notify_one();
      } else {
        // 移除无效连接
        removeConnection(pool, pooled_conn);
        std::cout
            << "[ConnectionPool] Removed invalid connection during release to "
            << host << ":" << port << std::endl;
      }
    } else {
      std::cout << "[ConnectionPool] ⚠ Connection was not marked as in use: "
                << host << ":" << port << std::endl;
    }
  } else {
    std::cout << "[ConnectionPool] ⚠ Connection not found in pool for " << host
              << ":" << port << std::endl;
    conn->close();
  }
}

// 其余私有方法的实现和之前完全相同...
// (为了节省空间，这里不重复所有实现)

std::string ConnectionPool::makeKey(const std::string& host, int port) const {
  return host + ":" + std::to_string(port);
}

ConnectionPool::HostPool* ConnectionPool::getOrCreateHostPool(
    const std::string& key) {
  {
    std::shared_lock<std::shared_mutex> shared_lock(pools_mutex_);
    auto it = pools_.find(key);
    if (it != pools_.end()) {
      return it->second.get();
    }
  }

  std::unique_lock<std::shared_mutex> unique_lock(pools_mutex_);
  auto it = pools_.find(key);
  if (it != pools_.end()) {
    return it->second.get();
  }

  auto pool = std::make_unique<HostPool>();
  HostPool* pool_ptr = pool.get();
  pools_[key] = std::move(pool);

  std::cout << "[ConnectionPool] Created host pool for " << key << std::endl;
  return pool_ptr;
}

std::shared_ptr<TcpConnection> ConnectionPool::createConnection(
    const std::string& host, int port) {
  try {
    auto start_time = std::chrono::steady_clock::now();

    // 创建socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      std::cerr << "[ConnectionPool] Failed to create socket: "
                << strerror(errno) << std::endl;
      return nullptr;
    }

    // 设置连接超时
    struct timeval timeout;
    timeout.tv_sec = config_.connection_timeout.count();
    timeout.tv_usec = 0;

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) <
        0) {
      std::cerr << "[ConnectionPool] Failed to set receive timeout: "
                << strerror(errno) << std::endl;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) <
        0) {
      std::cerr << "[ConnectionPool] Failed to set send timeout: "
                << strerror(errno) << std::endl;
    }

    // 设置TCP_NODELAY以减少延迟
    int flag = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
      std::cerr << "[ConnectionPool] Failed to set TCP_NODELAY: "
                << strerror(errno) << std::endl;
    }

    // 设置地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
      std::cerr << "[ConnectionPool] Invalid address: " << host << std::endl;
      close(fd);
      return nullptr;
    }

    // 连接
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
      std::cerr << "[ConnectionPool] Failed to connect to " << host << ":"
                << port << ": " << strerror(errno) << std::endl;
      close(fd);
      return nullptr;
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    auto conn = std::make_shared<TcpConnection>(fd);

    std::cout << "[ConnectionPool] ✓ Created connection to " << host << ":"
              << port << " in " << duration.count() << "ms" << std::endl;

    return conn;

  } catch (const std::exception& e) {
    std::cerr << "[ConnectionPool] Exception creating connection to " << host
              << ":" << port << ": " << e.what() << std::endl;
    return nullptr;
  }
}

bool ConnectionPool::isConnectionValid(
    const std::shared_ptr<TcpConnection>& conn) {
  if (!conn || !conn->isConnected()) {
    return false;
  }

  // 可以添加更详细的健康检查
  int error = 0;
  socklen_t len = sizeof(error);
  int fd = conn->getFd();

  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
    return false;
  }

  return error == 0;
}

void ConnectionPool::removeConnection(
    HostPool* pool, std::shared_ptr<PooledConnection> pooled_conn) {
  // 从all_connections中移除
  auto it =
      std::find_if(pool->all_connections.begin(), pool->all_connections.end(),
                   [&](const std::shared_ptr<PooledConnection>& conn) {
                     return conn.get() == pooled_conn.get();
                   });

  if (it != pool->all_connections.end()) {
    pool->all_connections.erase(it);
  }

  // 关闭连接
  if (pooled_conn->connection) {
    pooled_conn->connection->close();
  }
}

// 其余方法的实现和之前相同...
ConnectionPool::Stats ConnectionPool::getStats() const {
  Stats stats;

  std::shared_lock<std::shared_mutex> lock(pools_mutex_);

  stats.hosts_count = pools_.size();

  for (const auto& pool_pair : pools_) {
    const HostPool* pool = pool_pair.second.get();
    std::unique_lock<std::mutex> pool_lock(pool->mutex);

    stats.total_connections += pool->getTotalConnections();
    stats.active_connections += pool->getActiveConnections();
    stats.idle_connections += pool->getIdleConnections();
  }

  return stats;
}

void ConnectionPool::warmup(const std::string& host, int port, size_t count) {
  if (count == 0) {
    count = config_.min_connections_per_host;
  }

  std::cout << "[ConnectionPool] Warming up " << count << " connections to "
            << host << ":" << port << std::endl;

  std::vector<std::shared_ptr<TcpConnection>> connections;
  connections.reserve(count);

  for (size_t i = 0; i < count; ++i) {
    try {
      auto conn = acquire(host, port);
      if (conn) {
        connections.push_back(conn);
      }
    } catch (const std::exception& e) {
      std::cerr << "[ConnectionPool] Failed to warmup connection " << (i + 1)
                << "/" << count << " to " << host << ":" << port << ": "
                << e.what() << std::endl;
      break;
    }
  }

  // 释放所有连接
  for (auto& conn : connections) {
    release(host, port, conn);
  }

  std::cout << "[ConnectionPool] ✓ Warmed up " << connections.size() << "/"
            << count << " connections to " << host << ":" << port << std::endl;
}

void ConnectionPool::cleanup() { cleanupExpiredConnections(); }

void ConnectionPool::cleanupExpiredConnections() {
  while (running_) {
    // 每隔30秒执行一次清理
    std::this_thread::sleep_for(std::chrono::seconds(30));

    if (!running_) break;

    std::cout << "[ConnectionPool] 🧹 Starting cleanup cycle..." << std::endl;

    // 为了避免长时间锁定主池，先复制一份key
    std::vector<std::string> keys;
    {
      std::shared_lock<std::shared_mutex> lock(pools_mutex_);
      for (const auto& pair : pools_) {
        keys.push_back(pair.first);
      }
    }

    for (const auto& key : keys) {
      HostPool* pool = nullptr;
      {
        std::shared_lock<std::shared_mutex> lock(pools_mutex_);
        auto it = pools_.find(key);
        if (it != pools_.end()) {
          pool = it->second.get();
        }
      }

      if (!pool) continue;

      // 对每个HostPool进行清理
      std::vector<std::shared_ptr<PooledConnection>> to_remove;
      {
        std::unique_lock<std::mutex> lock(pool->mutex);

        // 使用std::remove_if的惯用法来标记和移动待删除的元素
        auto new_end = std::remove_if(
            pool->all_connections.begin(), pool->all_connections.end(),
            [&](const std::shared_ptr<PooledConnection>& p_conn) {
              if (p_conn->in_use) {
                return false;  // 正在使用的连接不能清理
              }

              auto idle_duration =
                  std::chrono::steady_clock::now() - p_conn->last_used;
              bool expired = idle_duration > config_.idle_timeout;
              // 如果连接数大于最小值，就可以考虑回收
              bool over_min = pool->getTotalConnections() >
                              config_.min_connections_per_host;

              if (expired || over_min) {
                std::cout << "[ConnectionPool] 🧹 Marking connection to " << key
                          << " for removal (expired: " << expired
                          << ", over_min: " << over_min << ")" << std::endl;
                // 标记为待删除，但暂时不关闭，避免死锁
                return true;
              }
              return false;
            });

        // 将待删除的元素移动到 to_remove 向量中，以便在锁外关闭
        for (auto it = new_end; it != pool->all_connections.end(); ++it) {
          to_remove.push_back(*it);
        }

        // 真正从all_connections中擦除
        pool->all_connections.erase(new_end, pool->all_connections.end());

        // 更新空闲队列，确保它与all_connections同步
        std::queue<std::shared_ptr<PooledConnection>> new_idle_queue;
        for (const auto& conn : pool->all_connections) {
          if (!conn->in_use) {
            new_idle_queue.push(conn);
          }
        }
        pool->idle_connections.swap(new_idle_queue);
      }

      // 在锁之外安全地关闭连接
      // to_remove中的shared_ptr将在这里被析构，从而释放TcpConnection
    }
  }
  std::cout << "[ConnectionPool] 🧹 Cleanup thread finished." << std::endl;
}

void ConnectionPool::runCleanupLoop() {
  std::cout << "[ConnectionPool] Cleanup thread started" << std::endl;

  while (running_) {
    try {
      std::this_thread::sleep_for(std::chrono::seconds(60));  // 每分钟清理一次

      if (running_) {
        cleanupExpiredConnections();
      }
    } catch (const std::exception& e) {
      std::cerr << "[ConnectionPool] Cleanup thread error: " << e.what()
                << std::endl;
    }
  }

  std::cout << "[ConnectionPool] Cleanup thread stopped" << std::endl;
}

void ConnectionPool::printStats() const {
  auto stats = getStats();

  std::cout << "\n=== Connection Pool Statistics ===" << std::endl;
  std::cout << "Total hosts: " << stats.hosts_count << std::endl;
  std::cout << "Total connections: " << stats.total_connections << std::endl;
  std::cout << "Active connections: " << stats.active_connections << std::endl;
  std::cout << "Idle connections: " << stats.idle_connections << std::endl;
  std::cout << "Pool utilization: " << std::fixed << std::setprecision(1)
            << (stats.total_connections > 0
                    ? (100.0 * stats.active_connections /
                       stats.total_connections)
                    : 0.0)
            << "%" << std::endl;

  // 详细的每个主机池信息
  std::shared_lock<std::shared_mutex> lock(pools_mutex_);
  for (const auto& pool_pair : pools_) {
    const std::string& key = pool_pair.first;
    const HostPool* pool = pool_pair.second.get();
    std::unique_lock<std::mutex> pool_lock(pool->mutex);

    std::cout << "  " << key << ": " << pool->getActiveConnections() << "/"
              << pool->getTotalConnections()
              << " active (idle: " << pool->getIdleConnections() << ")"
              << std::endl;
  }
  std::cout << "=================================\n" << std::endl;
}
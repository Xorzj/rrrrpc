#ifndef EPOLL_SERVER_H
#define EPOLL_SERVER_H

#include <sys/epoll.h>

#include <atomic>
#include <functional>
#include <memory>
#include <queue>
#include <shared_mutex>
#include <unordered_map>

#include "network.h"
#include "thread_pool.h"

class EpollServer {
 public:
  using ConnectionHandler =
      std::function<void(int fd, const std::string& data)>;
  using ConnectionCloseHandler = std::function<void(int fd)>;

  struct Config {
    int max_events = 1024;
    int epoll_timeout = 1000;  // ms
    size_t thread_pool_size = std::thread::hardware_concurrency();
    size_t max_connections = 10000;
    size_t buffer_size = 4096;
  };
  // 构造函数 - 提供两个版本避免默认参数问题
  explicit EpollServer(int port);
  EpollServer(int port, const Config& config);
  ~EpollServer();

  // 禁用拷贝和移动
  EpollServer(const EpollServer&) = delete;
  EpollServer& operator=(const EpollServer&) = delete;
  EpollServer(EpollServer&&) = delete;
  EpollServer& operator=(EpollServer&&) = delete;

  bool start();
  void stop();
  void run();

  void setConnectionHandler(ConnectionHandler handler);
  void setConnectionCloseHandler(ConnectionCloseHandler handler);

  // 发送响应给客户端
  bool sendResponse(int client_fd, const std::string& data);

  // 获取统计信息
  struct Stats {
    size_t total_connections = 0;
    size_t active_connections = 0;
    size_t total_requests = 0;
    size_t failed_requests = 0;
    size_t bytes_received = 0;
    size_t bytes_sent = 0;
  };
  Stats getStats() const;

 private:
  struct ClientConnection {
    int fd;
    std::string receive_buffer;
    std::queue<std::string> send_queue;
    std::mutex send_mutex;
    std::chrono::steady_clock::time_point last_activity;
    bool closed = false;

    explicit ClientConnection(int f)
        : fd(f), last_activity(std::chrono::steady_clock::now()) {}
  };

  int port_;
  int listen_fd_;
  int epoll_fd_;
  Config config_;

  std::atomic<bool> running_;
  std::unique_ptr<ThreadPool> thread_pool_;

  // 连接管理
  std::unordered_map<int, std::shared_ptr<ClientConnection>> connections_;
  std::shared_mutex connections_mutex_;

  // 回调函数
  ConnectionHandler connection_handler_;
  ConnectionCloseHandler connection_close_handler_;

  // 统计信息
  mutable std::atomic<size_t> total_connections_{0};
  mutable std::atomic<size_t> active_connections_{0};
  mutable std::atomic<size_t> total_requests_{0};
  mutable std::atomic<size_t> failed_requests_{0};
  mutable std::atomic<size_t> bytes_received_{0};
  mutable std::atomic<size_t> bytes_sent_{0};

  // 内部方法
  bool setupListenSocket();
  bool setupEpoll();
  void handleNewConnection();
  void handleClientData(int client_fd);
  void handleClientWrite(int client_fd);
  void closeConnection(int client_fd);
  bool setNonBlocking(int fd);

  // 消息解析（处理TCP粘包）
  std::vector<std::string> extractMessages(
      std::shared_ptr<ClientConnection> conn, const std::string& new_data);
};

#endif
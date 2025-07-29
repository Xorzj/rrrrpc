#include "epoll_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

// 默认构造函数
EpollServer::EpollServer(int port) : EpollServer(port, Config{}) {}

// 带配置的构造函数
EpollServer::EpollServer(int port, const Config& config)
    : port_(port),
      listen_fd_(-1),
      epoll_fd_(-1),
      config_(config),
      running_(false) {
  std::cout << "[EpollServer] Creating server on port " << port_ << " with "
            << config_.thread_pool_size << " worker threads" << std::endl;
  std::cout << "[EpollServer] Max events: " << config_.max_events << std::endl;
  std::cout << "[EpollServer] Max connections: " << config_.max_connections
            << std::endl;
  std::cout << "[EpollServer] Buffer size: " << config_.buffer_size << " bytes"
            << std::endl;

  thread_pool_ = std::make_unique<ThreadPool>(config_.thread_pool_size);
}

EpollServer::~EpollServer() { stop(); }

bool EpollServer::start() {
  std::cout << "[EpollServer] Starting server..." << std::endl;

  if (!setupListenSocket()) {
    std::cerr << "[EpollServer] Failed to setup listen socket" << std::endl;
    return false;
  }

  if (!setupEpoll()) {
    std::cerr << "[EpollServer] Failed to setup epoll" << std::endl;
    close(listen_fd_);
    return false;
  }

  running_ = true;
  std::cout << "[EpollServer] ✓ Server started on port " << port_ << std::endl;
  return true;
}

void EpollServer::stop() {
  if (!running_) return;

  std::cout << "[EpollServer] Stopping server..." << std::endl;
  running_ = false;

  // 关闭所有客户端连接
  {
    std::unique_lock<std::shared_mutex> lock(connections_mutex_);
    for (auto& pair : connections_) {
      close(pair.first);
    }
    connections_.clear();
  }

  if (epoll_fd_ >= 0) {
    close(epoll_fd_);
    epoll_fd_ = -1;
  }

  if (listen_fd_ >= 0) {
    close(listen_fd_);
    listen_fd_ = -1;
  }

  thread_pool_.reset();
  std::cout << "[EpollServer] Server stopped" << std::endl;
}

void EpollServer::run() {
  std::cout << "[EpollServer] Starting event loop..." << std::endl;

  std::vector<epoll_event> events(config_.max_events);

  while (running_) {
    int nfds = epoll_wait(epoll_fd_, events.data(), config_.max_events,
                          config_.epoll_timeout);

    if (nfds == -1) {
      if (errno == EINTR) continue;  // 被信号中断
      std::cerr << "[EpollServer] epoll_wait failed: " << strerror(errno)
                << std::endl;
      break;
    }

    for (int i = 0; i < nfds; ++i) {
      const auto& event = events[i];
      int fd = event.data.fd;

      if (fd == listen_fd_) {
        // 新连接
        if (event.events & EPOLLIN) {
          handleNewConnection();
        }
      } else {
        // 客户端事件
        if (event.events & EPOLLIN) {
          handleClientData(fd);
        } else if (event.events & EPOLLOUT) {
          handleClientWrite(fd);
        } else if (event.events & (EPOLLHUP | EPOLLERR)) {
          closeConnection(fd);
        }
      }
    }
  }

  std::cout << "[EpollServer] Event loop stopped" << std::endl;
}

void EpollServer::setConnectionHandler(ConnectionHandler handler) {
  connection_handler_ = handler;
}

void EpollServer::setConnectionCloseHandler(ConnectionCloseHandler handler) {
  connection_close_handler_ = handler;
}

bool EpollServer::sendResponse(int client_fd, const std::string& data) {
  std::shared_lock<std::shared_mutex> lock(connections_mutex_);
  auto it = connections_.find(client_fd);
  if (it == connections_.end()) {
    return false;
  }

  auto conn = it->second;
  lock.unlock();

  std::lock_guard<std::mutex> send_lock(conn->send_mutex);

  // 添加长度前缀（4字节，网络字节序）
  uint32_t len = htonl(data.length());
  std::string message;
  message.append(reinterpret_cast<const char*>(&len), sizeof(len));
  message.append(data);

  ssize_t sent =
      send(client_fd, message.data(), message.length(), MSG_NOSIGNAL);
  if (sent > 0) {
    bytes_sent_ += sent;
    return sent == static_cast<ssize_t>(message.length());
  }

  return false;
}

EpollServer::Stats EpollServer::getStats() const {
  Stats stats;
  stats.total_connections = total_connections_.load();
  stats.active_connections = active_connections_.load();
  stats.total_requests = total_requests_.load();
  stats.failed_requests = failed_requests_.load();
  stats.bytes_received = bytes_received_.load();
  stats.bytes_sent = bytes_sent_.load();
  return stats;
}

// 私有方法实现
bool EpollServer::setupListenSocket() {
  listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    std::cerr << "[EpollServer] Failed to create socket: " << strerror(errno)
              << std::endl;
    return false;
  }

  // 设置端口重用
  int opt = 1;
  if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    std::cerr << "[EpollServer] Failed to set SO_REUSEADDR: " << strerror(errno)
              << std::endl;
    return false;
  }

  // 设置非阻塞
  if (!setNonBlocking(listen_fd_)) {
    std::cerr << "[EpollServer] Failed to set listen socket non-blocking"
              << std::endl;
    return false;
  }

  // 绑定地址
  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port_);

  if (bind(listen_fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
    std::cerr << "[EpollServer] Failed to bind: " << strerror(errno)
              << std::endl;
    return false;
  }

  // 开始监听
  if (listen(listen_fd_, 1024) < 0) {
    std::cerr << "[EpollServer] Failed to listen: " << strerror(errno)
              << std::endl;
    return false;
  }

  return true;
}

bool EpollServer::setupEpoll() {
  epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
  if (epoll_fd_ < 0) {
    std::cerr << "[EpollServer] Failed to create epoll: " << strerror(errno)
              << std::endl;
    return false;
  }

  // 添加监听socket到epoll
  epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = listen_fd_;

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev) < 0) {
    std::cerr << "[EpollServer] Failed to add listen fd to epoll: "
              << strerror(errno) << std::endl;
    return false;
  }

  return true;
}

void EpollServer::handleNewConnection() {
  while (true) {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(listen_fd_, (sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;  // 没有更多连接
      }
      std::cerr << "[EpollServer] Accept failed: " << strerror(errno)
                << std::endl;
      break;
    }

    // 检查连接数限制
    if (active_connections_.load() >= config_.max_connections) {
      std::cout
          << "[EpollServer] Connection limit reached, rejecting new connection"
          << std::endl;
      close(client_fd);
      continue;
    }

    // 设置客户端socket非阻塞
    if (!setNonBlocking(client_fd)) {
      std::cerr << "[EpollServer] Failed to set client socket non-blocking"
                << std::endl;
      close(client_fd);
      continue;
    }

    // 添加到epoll
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  // 边缘触发
    ev.data.fd = client_fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
      std::cerr << "[EpollServer] Failed to add client fd to epoll: "
                << strerror(errno) << std::endl;
      close(client_fd);
      continue;
    }

    // 创建连接对象
    auto conn = std::make_shared<ClientConnection>(client_fd);

    {
      std::unique_lock<std::shared_mutex> lock(connections_mutex_);
      connections_[client_fd] = conn;
    }

    ++total_connections_;
    ++active_connections_;

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    std::cout << "[EpollServer] New connection from " << client_ip << ":"
              << ntohs(client_addr.sin_port) << " (fd=" << client_fd
              << ", active=" << active_connections_.load() << ")" << std::endl;
  }
}

void EpollServer::handleClientData(int client_fd) {
  std::shared_lock<std::shared_mutex> lock(connections_mutex_);
  auto it = connections_.find(client_fd);
  if (it == connections_.end()) {
    return;
  }

  auto conn = it->second;
  lock.unlock();

  if (conn->closed) {
    return;
  }

  char buffer[config_.buffer_size];
  ssize_t bytes_read;

  while ((bytes_read = recv(client_fd, buffer, sizeof(buffer), 0)) > 0) {
    bytes_received_ += bytes_read;
    conn->last_activity = std::chrono::steady_clock::now();

    // 添加到接收缓冲区
    conn->receive_buffer.append(buffer, bytes_read);

    // 尝试解析完整消息
    auto messages = extractMessages(conn, "");

    // 处理每个完整消息
    for (const auto& message : messages) {
      ++total_requests_;

      if (connection_handler_) {
        // 在线程池中处理业务逻辑
        thread_pool_->enqueue([this, client_fd, message]() {
          try {
            connection_handler_(client_fd, message);
          } catch (const std::exception& e) {
            std::cerr << "[EpollServer] Handler error: " << e.what()
                      << std::endl;
            ++failed_requests_;
          }
        });
      }
    }
  }

  if (bytes_read == 0) {
    // 客户端关闭连接
    closeConnection(client_fd);
  } else if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    // 读取错误
    std::cerr << "[EpollServer] Read error on fd " << client_fd << ": "
              << strerror(errno) << std::endl;
    closeConnection(client_fd);
  }
}

void EpollServer::handleClientWrite(int client_fd) {
  // 处理写事件（如果需要的话）
  // 当前实现中，我们在sendResponse中直接写入
}

void EpollServer::closeConnection(int client_fd) {
  std::unique_lock<std::shared_mutex> lock(connections_mutex_);
  auto it = connections_.find(client_fd);
  if (it == connections_.end()) {
    return;
  }

  auto conn = it->second;
  if (conn->closed) {
    return;
  }

  conn->closed = true;
  connections_.erase(it);
  lock.unlock();

  // 从epoll中移除
  epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);

  // 关闭socket
  close(client_fd);

  --active_connections_;

  std::cout << "[EpollServer] Connection closed (fd=" << client_fd
            << ", active=" << active_connections_.load() << ")" << std::endl;

  if (connection_close_handler_) {
    connection_close_handler_(client_fd);
  }
}

bool EpollServer::setNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }

  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

std::vector<std::string> EpollServer::extractMessages(
    std::shared_ptr<ClientConnection> conn, const std::string& new_data = "") {
  std::vector<std::string> messages;

  while (conn->receive_buffer.length() >= sizeof(uint32_t)) {
    // 读取消息长度
    uint32_t message_len;
    memcpy(&message_len, conn->receive_buffer.data(), sizeof(uint32_t));
    message_len = ntohl(message_len);

    // 检查是否接收到完整消息
    size_t total_len = sizeof(uint32_t) + message_len;
    if (conn->receive_buffer.length() < total_len) {
      break;  // 消息不完整
    }

    // 提取消息
    std::string message =
        conn->receive_buffer.substr(sizeof(uint32_t), message_len);
    messages.push_back(message);

    // 从缓冲区移除已处理的消息
    conn->receive_buffer.erase(0, total_len);
  }

  return messages;
}
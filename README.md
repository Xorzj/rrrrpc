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

# RPC框架

## Cmake和make相关：

```bash
Error:
CMake Error: The current CMakeCache.txt directory /home/xorzj/rpc_framework/build/CMakeCache.txt is different than the directory /home/xorzjxie/rpc_framework/build where CMakeCache.txt was created. This may result in binaries being created in the wrong place. If you are not sure, reedit the CMakeCache.txt
CMake Error: The source "/home/xorzj/rpc_framework/CMakeLists.txt" does not match the source "/home/xorzjxie/rpc_framework/CMakeLists.txt" used to generate cache.  Re-run cmake with a different source directory.
CMake Error: The source directory "/home/xorzjxie/rpc_framework" does not exist.
```

Cmake是蓝图会指导make生成文件，make中会记录编译缓存和一些编译检查，如果你切换了位置，编译检查记录就对不上，就会报错，应该让删除build文件夹，最好的是不上传build文件夹。

记得将build目录添加到 .gitignore

## 如何安装Protocol Buffer

使用apt安装；

两个核心库

`protobuf-compiler`: 包含 `protoc` 编译器，用于将 `.proto` 文件转换成特定语言的代码。

`libprotobuf-dev`: 包含了 Protobuf 的头文件和静态库，在 C++ 项目中编译时需要链接它们。

`sudo apt install protobuf-compiler libprotobuf-dev` 

protoc 只是一个代码生成工具，还需要为编程语言选择特定的运行时库。

## 一些Cmake,Make语法

`make -j$(nproc)` ：使用全部的CPU核心用于编译

nproc是打印当前可用CPU

-j 是同时执行多少个编译任务，开多少个线程。

## Tmux

ctrl b + `%` 左右切割

ctrl b + `"`  上下切割

ctrl b (按住) ＋上下左右 切换窗口尺寸

ctrl b + `:` 开启命令台，输入 `set-option -g mouse on` 可以开启鼠标控制模式

## 一些网络编程函数：

`htonl` (Host to Network Long) 将32位的Long从主机字节序转为网络字节序（大端序），发送IPv4地址(32位)

`htons` 将16位的short 转换 发送端口

`ntohs` 网络序变主机序 

**大端序 (Big-Endian)**：**高位字节**存放在**低地址**

数字 `0x12345678` 在内存中存储为：`12 34 56 78`

数字 `0x12345678` 在内存中存储为：`78 56 34 12`

```c++
#include <sys/socket.h>

ssize_t send(int sockfd, const void *buf, size_t len, int flags);
// sockfd: 套接字fd，确定数据发到哪个连接上。
// buf: 缓冲区指针
// len: 发送字节数
// flags: 标志特性 
// MSG_DONTWAIT 进行非阻塞操作
// MSG_NOSIGNAL: 当连接被对方关闭时，不要发送 SIGPIPE 信号（这通常会导致程序终止），而是让 send 调用返回一个错误

// return_value
//返回正数: 成功发送的字节数。

//注意: 这个返回值可能小于你要求发送的 len。这不一定是错误，可能只是因为底层的发送缓冲区满了。健壮的程序必须检查返回值，并通过一个循环来确保所有数据都被发送出去。

//返回 -1: 发生错误。
// EAGAIN 发送缓冲区满
//处理: 此时，你需要检查全局变量 errno 来获取具体的错误码，例如 ECONNRESET (连接被对方重置) 或 EPIPE (管道破裂，对方已关闭连接)。
#include <sys/socket.h>

ssize_t recv(int sockfd, void *buf, size_t len, int flags);
// flags:MSG_DONTWAIT 如果接收缓冲区没有数据，函数会立即返回错误 (EAGAIN 或 EWOULDBLOCK) 而不是等待
// EAGAIN 接受缓冲区空
// MSG_PEEK : “窥探”数据。函数会将缓冲区的数据复制到你的 buf 中，但不会将这些数据从系统接收缓冲区中移除。这意味着下一次调用 recv (不带 MSG_PEEK) 仍然能读到相同的数据。这在需要检查消息头以确定如何处理消息体时非常有用。

// 返回 正数 收到的字节
// 返回 0 连接由对方关闭 .close()

```

```c++
#include <sys/socket.h> // On Linux/macOS
#include <sys/types.h>  // On Linux/macOS
// #include <winsock2.h> // On Windows

int tcp_socket = socket(
    /* domain: int */      AF_INET,      // AF_INET: 使用 IPv4 协议族。 (其他如 AF_INET6 用于 IPv6)
    /* type: int */        SOCK_STREAM,  // SOCK_STREAM: 创建一个流式套接字，用于 TCP 协议。(其他如 SOCK_DGRAM 用于 UDP)(SOCK_RAW 原始套接字，IP ICMP)
    /* protocol: int */    0             // 0: 根据前两个参数自动选择默认协议。(此处会自动选择 TCP)
);

// ===== 返回值检查 (Return Value Check) =====
//  - 成功 (Success): 返回一个非负整数，即新的套接字文件描述符 (e.g., tcp_socket > 0)。
//  - 失败 (Failure): 返回 -1，并设置全局变量 errno。
if (tcp_socket == -1) {
    perror("socket creation failed");
}

#include <sys/socket.h>

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
/*
int level
含义：选项的“级别”或“层面”。它告诉函数，你要设置的选项是属于哪个协议层面的。
常用值：
SOL_SOCKET: 这是最常用的级别，表示要设置的选项是套接字层面的通用选项，不针对任何特定的协议（如TCP或IP），而是对所有类型的套接字都可能适用。你的例子中 SO_REUSEADDR 就属于这个层面。
IPPROTO_TCP: 表示选项是专门针对 TCP 协议的，例如 TCP_NODELAY (禁用Nagle算法)。
IPPROTO_IP: 表示选项是针对 IP 协议的，例如 IP_TTL (设置IP包的生存时间)。
*/
int result = setsockopt(
    /* 参数1: int sockfd */      listen_fd_,   // 要进行配置的套接字文件描述符，这里是服务器的监听套接字。
    
    /* 参数2: int level */       SOL_SOCKET,   // 选项级别：SOL_SOCKET 表示这是一个通用的套接字层面选项。
    
    /* 参数3: int optname */     SO_REUSEADDR, // 选项名称：SO_REUSEADDR (Socket Option - REUSE ADDRess)，即地址复用选项。
    
    /* 参数4: const void* optval */ &opt,         // 选项值指针：指向我们准备好的值为 1 的变量 opt。
    
    /* 参数5: socklen_t optlen */ sizeof(opt)    // 选项值长度：opt 变量所占的字节数。
);

//SO_REUSEADDR  选项
/* 
问题：如果你的服务器程序崩溃或重启，当它尝试重新 bind() 到之前的端口（比如 8080）时，如果恰好有旧的连接还处于 TIME_WAIT 状态，操作系统会认为这个端口“仍被占用”，导致 bind() 失败，并报告 "Address already in use" 错误。你就必须等待几分钟才能成功重启服务器，这在生产环境中是不可接受的。

解决方案：setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); 这行代码就是告诉操作系统内核：“请允许我重用处于 TIME_WAIT 状态的地址和端口。”
*/

```

```c++
// 设置文件描述符的阻塞
// int fcntl(int fd, int cmd, ... /* arg */ );
// cmd 有 F_GETFL F_SETFL
bool EpollServer::setNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }

  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}
```

```c++
struct sockaddr_in {
    sa_family_t     sin_family;    // 地址族 (Address Family) 一般写AF_INET
    in_port_t       sin_port;      // 端口号 (Port number) //记得htons 
    struct in_addr  sin_addr;      // IPv4 地址 内部放着一个字段s_addr (一个uint32_t)
    unsigned char   sin_zero[8];   // 填充位，为了与 sockaddr 结构体大小保持一致
};
```

`bind , listen , accept`

```c++
#include <sys/socket.h>
INET_ADDRSTRLEN 地址的长度宏定义。

int inet_pton(sin_family,SERVER_IP,&目标位置)//将点分制转成网络字节序
    
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

int listen(int sockfd, int backlog);
//sockfd: 已经成功 bind() 的套接字文件描述符。

//backlog: 等待队列的最大长度。这是一个非常重要的参数。它定义了内核为这个套接字所维护的“已完成连接但尚未被应用程序 accept()”的队列大小。当客户端的连接请求到达时，内核会完成TCP三次握手，然后将这个连接放入一个队列中，等待应用程序调用 accept() 来取走。如果这个队列满了，新的连接请求可能会被拒绝或忽略。这个值不是指最大并发连接数，而是指“排队等候被accept”的连接数。
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
//sockfd: 正在监听的那个套接字。
//addr: 一个指向 sockaddr 结构体的指针，用于存放新连接进来的客户端的地址信息（IP和端口）。这样服务器就知道是哪个客户端连上来了。

//addrlen: addr 结构体的长度的指针。
//如果等待队列中没有连接，accept() 调用会阻塞
```

客户端 `connect`

```c++
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
sockfd: 由客户端自己调用 socket() 创建的文件描述符。

addr: 一个指向 sockaddr 结构体的指针，里面包含了服务器的地址和端口信息。客户端必须准确填充这个结构体，才能找到正确的服务器。

addrlen: addr 结构体的长度。
```

## 一个简单的Server和Client的示范

### Server

```c++
const std::string kSERVER_ADDR = "127.0.0.1";
const int kSERVER_PORT = 8080;
int main() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  int reuse = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  //wrong:setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, 1);
  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(kSERVER_PORT);
  if (inet_pton(AF_INET, kSERVER_ADDR.c_str(), &addr.sin_addr) <= 0) {
    std::cout << "The server addr is error !" << std::endl;
    return -1;
  }
  if (bind(fd, (sockaddr*)(&addr), sizeof(addr)) < 0) {
    std::cout << "Bind is error" << std::endl;
    return -1;
  }
  std::cout << "服务器已经绑定到8080端口" << std::endl;
  listen(fd, 5);
  while (true) {
    sockaddr client_addr;
    socklen_t client_len = sizeof(client_addr);
    // wrong:socklen_t client_len;
    int client_fd = accept(fd, &client_addr, &client_len);
    if (client_fd < 0) {
      std::cout << "accept error" << std::endl;
    }
    // 指针操纵强转,不存在类型强转
    const sockaddr* clientA = (const sockaddr*)(&client_addr);
    const sockaddr_in* client_I = (const sockaddr_in*)(clientA);
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_I->sin_addr, client_ip, INET_ADDRSTRLEN);

    std::string message = "Hello Client from " + std::string(client_ip) +
                          std::to_string(ntohs(client_I->sin_port));
    int len = message.length();
    int net_len = htonl(len);
    // 注意htonl的转换
    int send_bytes = send(client_fd, &net_len, sizeof(net_len), MSG_NOSIGNAL);
    if (send_bytes < 0) {
      perror("send header failed");
    }
    std::cout << message << std::endl;
    int u = send(client_fd, message.c_str(), message.size(), MSG_NOSIGNAL);
    if (u == -1) {
      std::cout << "client is close " << std::endl;
    }
    close(client_fd);
  }
  close(fd);
  return 0;
}
```

```c++
const std::string kSERVER_IP = "127.0.0.1";
const int kSERVER_HOST = 8080;
int main() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in addr;
  addr.sin_family = AF_INET;
  inet_pton(AF_INET, kSERVER_IP.c_str(), &addr.sin_addr);
  addr.sin_port = htons(kSERVER_HOST);
  if (connect(fd, (sockaddr*)(&addr), sizeof(addr)) == -1) {
    std::cout << "Connect error" << std::endl;
    return -1;
  }
  std::cout << "Connect is Start" << std::endl;
  int size = 0;
  std::vector<char> msg_head(4);
  while (size < 4) {
    int read_bytes = recv(fd, msg_head.data() + size, 4 - size, 0);
    std::cout << "read_bytes:" << read_bytes << std::endl;
    if (read_bytes <= 0) {
      std::cout << "Read Error" << std::endl;
      break;
    }
    size += read_bytes;
  }
  int raw_len = 0;
  memcpy(&raw_len, msg_head.data(), 4);
  int len = ntohl(raw_len);
  std::vector<char> body(len);
  size = 0;
  while (size < len) {
    int read_bytes = recv(fd, body.data() + size, len - size, 0);
    if (read_bytes <= 0) {
      std::cout << "Read Error" << std::endl;
      break;
    }
    size += read_bytes;
  }
  std::cout << std::string(body.data(), len) << std::endl;
  std::string response = "message already recv";
  int send_len = htonl(response.size());
  send(fd, &send_len, 4, MSG_NOSIGNAL);
  send(fd, response.data(), response.size(), MSG_NOSIGNAL);
  close(fd);
  return 0;
}
```



## 一个使用IO多路复用的实现

首先介绍一下 **EPOLL**

`epoll` 通过内核与应用程序共享内存的方式，避免了 `select`/`poll` 那样每次调用都需要从用户空间向内核空间复制大量FD集合的开销。

### `epoll` 的核心API（三剑客）

使用 `epoll` 主要涉及三个系统调用：

1. #### `epoll_create1(flags)`

   - **作用**：创建一个 `epoll` 实例。你可以把它想象成创建一个“监控中心”。
   - **返回值**：一个文件描述符（`epoll_fd`），指向内核中的 `epoll` 数据结构。后续所有操作都通过这个 `epoll_fd` 进行。
   - `flags`：通常设置为 `EPOLL_CLOEXEC`，表示当程序执行 `exec` 系列调用时，自动关闭这个 `epoll_fd`，这是一个好习惯。

   ```c++
   int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
   ```

2. #### `epoll_ctl(epoll_fd, op, fd, event)`

   - **作用**：控制 `epoll` 实例，向“监控中心”**添加**、**修改**或**删除**需要监视的文件描述符。
   - `epoll_fd`：`epoll_create1` 返回的那个fd。
   - `op`：要执行的操作：
     - `EPOLL_CTL_ADD`：添加一个新的fd到监视列表。
     - `EPOLL_CTL_MOD`：修改一个已存在fd的监视事件。
     - `EPOLL_CTL_DEL`：从监视列表中删除一个fd。
   - `fd`：要操作的目标文件描述符（比如监听socket、客户端连接socket）。
   - `event`：一个指向 `struct epoll_event` 的指针，告诉 `epoll` 要关心这个 `fd` 上的哪些事件。

   **`struct epoll_event` 详解**:

   ```c++
   struct epoll_event {
       uint32_t      events;  // 要监视的事件类型 (位掩码)
       epoll_data_t  data;    // 用户数据，通常用来存fd
   };
   ```

   - `events`: 最常用的事件有：
     - `EPOLLIN`：fd上有数据可读。
     - `EPOLLOUT`：可以向fd写入数据而不会阻塞。
     - `EPOLLET`：**边缘触发**模式 (Edge-Triggered)。
     - `EPOLLERR` / `EPOLLHUP`：发生错误 / 连接被挂断。
   - `data`: 一个联合体，最常用的成员是 `data.fd`，我们通常把被监视的 `fd` 存进去。当 `epoll_wait` 返回时，我们可以从返回的事件中直接读出是哪个 `fd` 触发了事件。

3. #### `epoll_wait(epoll_fd, events, maxevents, timeout)`

   - **作用**：**等待事件发生**。这是服务器主循环的核心。程序会在这里阻塞，直到有被监视的fd上发生了我们感兴趣的事件，或者超时。
   - `epoll_fd`：我们的监控中心。
   - `events`：一个预先分配好的 `struct epoll_event` 数组的指针。内核会把触发了的事件信息填充到这个数组里。
   - `maxevents`：上面那个数组的大小，告诉内核一次最多可以返回多少个事件。
   - `timeout`：超时时间（毫秒）。-1 表示永久阻塞；0 表示立即返回（非阻塞）。
   - **返回值**：一个整数，表示有多少个fd已经就绪。返回0表示超时，-1表示出错。

设计原理希望不让IO操作阻塞，所以不要在循环体内部while等待recv，应该直接使用非阻塞。

```c++
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// --------------------------- 全局常量和辅助函数 ---------------------------

const std::string kSERVER_ADDR = "0.0.0.0";
const int kSERVER_PORT = 8080;
const int kMAX_EVENTS = 1024;
const int kBUFFER_SIZE = 4096;

int epoll_fd;
std::map<int, std::vector<char>> client_buffers;

void set_nonblocking(int fd) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

bool send_all(int fd, const void* data, size_t len) {
    const char* ptr = static_cast<const char*>(data);
    size_t total_sent = 0;
    while (total_sent < len) {
        ssize_t sent = send(fd, ptr + total_sent, len - total_sent, MSG_NOSIGNAL);
        if (sent <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::cerr << "send buffer is full on fd " << fd << ", will try later." << std::endl;
                // 在真实的服务器中，这里应该注册 EPOLLOUT 事件，稍后再发
                // 为简化，我们暂时认为发送失败
                return false;
            }
            perror("send failed");
            return false;
        }
        total_sent += sent;
    }
    return true;
}

bool send_message(int fd, const std::string& message) {
    uint32_t len = message.length();
    uint32_t net_len = htonl(len);
    if (!send_all(fd, &net_len, sizeof(net_len))) return false;
    if (!send_all(fd, message.data(), len)) return false;
    return true;
}

// --------------------------- 客户端数据处理逻辑 ---------------------------

void handle_disconnect(int fd) {
    std::cout << "Client on fd " << fd << " disconnected." << std::endl;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    client_buffers.erase(fd);
}

void handle_client_data(int fd) {
    char buffer[kBUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        client_buffers[fd].insert(client_buffers[fd].end(), buffer, buffer + bytes_read);
    }

    std::vector<char>& user_buffer = client_buffers[fd];
    if (user_buffer.size() >= sizeof(uint32_t)) {
        uint32_t msg_len;
        memcpy(&msg_len, user_buffer.data(), sizeof(uint32_t));
        msg_len = ntohl(msg_len);

        if (user_buffer.size() >= sizeof(uint32_t) + msg_len) {
            std::string message(user_buffer.data() + sizeof(uint32_t), msg_len);
            std::cout << "Received response from fd " << fd << ": \"" << message << "\"" << std::endl;
            // 你的客户端在发送完回执后就关闭了，所以服务器在收到后也应该关闭连接
            handle_disconnect(fd);
            return;
        }
    }

    if (bytes_read == 0) {
        handle_disconnect(fd);
    } else if (bytes_read < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("read error");
            handle_disconnect(fd);
        }
    }
}

// --------------------------- 主函数 ---------------------------

int main() {
    // 1. 初始化监听Socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return -1; }

    int reuse = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(kSERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind"); close(listen_fd); return -1;
    }

    if (listen(listen_fd, 128) < 0) {
        perror("listen"); close(listen_fd); return -1;
    }
    set_nonblocking(listen_fd);

    // 2. 创建epoll实例并注册监听事件
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) { perror("epoll_create1"); close(listen_fd); return -1; }

    epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event) < 0) {
        perror("epoll_ctl ADD listen_fd"); close(listen_fd); close(epoll_fd); return -1;
    }

    std::vector<epoll_event> events(kMAX_EVENTS);
    std::cout << "服务器已启动，正在监听 " << kSERVER_PORT << " 端口..." << std::endl;

    // 3. 事件主循环
    while (true) {
        int nfds = epoll_wait(epoll_fd, events.data(), kMAX_EVENTS, -1);
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            if (fd == listen_fd) {
                // --- 新连接处理 ---
                while (true) {
                    sockaddr_storage client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(listen_fd, (sockaddr*)&client_addr, &client_len);

                    if (client_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("accept");
                        break;
                    }
                    set_nonblocking(client_fd);
                    
                    // ✅ 修正: 打印客户端IP和端口
                    std::string client_ip_str = "unknown";
                    uint16_t client_port = 0;
                    if (client_addr.ss_family == AF_INET) {
                        auto* addr_in = (sockaddr_in*)&client_addr;
                        char ip_buf[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &addr_in->sin_addr, ip_buf, sizeof(ip_buf));
                        client_ip_str = ip_buf;
                        client_port = ntohs(addr_in->sin_port);
                    }
                    std::cout << "Accepted new connection from " << client_ip_str << ":" << client_port << " on fd " << client_fd << std::endl;
                    
                    // ✅ 修正: 立刻发送欢迎消息，打破死锁
                    std::string welcome_msg = "Hello " + client_ip_str + ", welcome to the server!";
                    if (!send_message(client_fd, welcome_msg)) {
                        std::cerr << "Failed to send welcome message to fd " << client_fd << std::endl;
                        close(client_fd);
                        continue;
                    }
                    
                    event.events = EPOLLIN | EPOLLET;
                    event.data.fd = client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == 0) {
                        client_buffers[client_fd] = std::vector<char>();
                    } else {
                        perror("epoll_ctl ADD client_fd");
                        close(client_fd);
                    }
                }
            } else if (events[i].events & EPOLLIN) {
                // --- 已连接客户端的数据处理 ---
                handle_client_data(fd);
            }
        }
    }

    close(listen_fd);
    close(epoll_fd);
    return 0;
}
```

```c++
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

// --------------------------- 全局常量和辅助函数 ---------------------------
const std::string kSERVER_IP = "127.0.0.1";
const int kSERVER_PORT = 8080;

// 辅助函数：确保发送指定长度的所有数据
bool send_all(int fd, const void* data, size_t len) {
    const char* ptr = static_cast<const char*>(data);
    size_t total_sent = 0;
    while (total_sent < len) {
        ssize_t sent = send(fd, ptr + total_sent, len - total_sent, 0);
        if (sent <= 0) {
            perror("send failed");
            return false;
        }
        total_sent += sent;
    }
    return true;
}

// 辅助函数：发送带4字节长度前缀的消息
bool send_message(int fd, const std::string& message) {
    uint32_t len = message.length();
    uint32_t net_len = htonl(len);
    if (!send_all(fd, &net_len, sizeof(net_len))) {
        return false;
    }
    if (!send_all(fd, message.data(), len)) {
        return false;
    }
    return true;
}

// 辅助函数：接收带4字节长度前缀的消息
// 使用 std::optional 在出错或连接关闭时返回一个空值
std::optional<std::string> receive_message(int fd) {
    // 1. 接收4字节的头部
    std::vector<char> header_buf(sizeof(uint32_t));
    ssize_t total_read = 0;
    while (total_read < sizeof(uint32_t)) {
        ssize_t bytes_read = recv(fd, header_buf.data() + total_read, sizeof(uint32_t) - total_read, 0);
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                std::cout << "Server closed connection while reading header." << std::endl;
            } else {
                perror("recv header failed");
            }
            return std::nullopt;
        }
        total_read += bytes_read;
    }

    // 2. 解析头部，获取消息体长度
    uint32_t msg_len;
    memcpy(&msg_len, header_buf.data(), sizeof(uint32_t));
    msg_len = ntohl(msg_len);

    // 3. 接收消息体
    if (msg_len == 0) {
        return ""; // 返回一个空字符串
    }
    
    std::vector<char> body_buf(msg_len);
    total_read = 0;
    while (total_read < msg_len) {
        ssize_t bytes_read = recv(fd, body_buf.data() + total_read, msg_len - total_read, 0);
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                std::cout << "Server closed connection while reading body." << std::endl;
            } else {
                perror("recv body failed");
            }
            return std::nullopt;
        }
        total_read += bytes_read;
    }

    return std::string(body_buf.data(), msg_len);
}


// --------------------------- 主函数 ---------------------------

int main() {
    // 1. 创建并连接到服务器
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kSERVER_PORT);
    if (inet_pton(AF_INET, kSERVER_IP.c_str(), &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(fd);
        return -1;
    }

    std::cout << "Connecting to server at " << kSERVER_IP << ":" << kSERVER_PORT << "..." << std::endl;
    if (connect(fd, (sockaddr*)(&addr), sizeof(addr)) == -1) {
        perror("connect error");
        close(fd);
        return -1;
    }
    std::cout << "Connection established." << std::endl;

    // 2. 接收来自服务器的欢迎消息
    std::cout << "Waiting for welcome message from server..." << std::endl;
    auto welcome_message = receive_message(fd);

    if (!welcome_message) {
        std::cerr << "Failed to receive welcome message." << std::endl;
        close(fd);
        return -1;
    }
    std::cout << "Received welcome message: \"" << *welcome_message << "\"" << std::endl;

    // 3. 发送确认回执给服务器
    std::string response = "Message received, thanks!";
    std::cout << "Sending response: \"" << response << "\"..." << std::endl;
    if (!send_message(fd, response)) {
        std::cerr << "Failed to send response." << std::endl;
        close(fd);
        return -1;
    }
    std::cout << "Response sent successfully." << std::endl;

    // 4. 关闭连接
    std::cout << "Closing connection." << std::endl;
    close(fd);
    return 0;
}
```



## 为什么使用send/recv 而不是read/write 

send/recv 的函数标准是

```c++
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
```

她们相比read/write 多了 一个flag可以实现很多具体细节

#### `recv()` 的常用 `flags`:

- **`MSG_PEEK`**: **窥探数据**。你可以使用这个标志查看接收缓冲区中的数据，但**不会将数据从缓冲区中移除**。下次再调用 `read` 或 `recv` 时，仍然可以读到相同的数据。这在需要根据数据内容决定如何处理，但又不希望立即消费数据时非常有用。`read` 完全没有这个能力。
- **`MSG_DONTWAIT`**: **单次非阻塞**。即使套接字本身是阻塞的，这次 `recv` 调用也会以非阻塞方式进行。如果缓冲区没有数据，它不会等待，而是立刻返回-1并设置 `errno` 为 `EAGAIN` 或 `EWOULDBLOCK`。这比用 `fcntl` 设置整个套接字的模式要更灵活。

#### `send()` 的常用 `flags`:

- **`MSG_NOSIGNAL`**: **抑制 `SIGPIPE` 信号**。这是一个在健壮的服务器编程中**极其重要**的标志。
  - **问题背景**: 当你尝试向一个已经关闭的套接字（例如，客户端已经断开连接）调用 `write` 或 `send` 时，默认情况下，内核会向你的进程发送一个 `SIGPIPE` 信号。这个信号的默认行为是**终止整个进程**。
  - **解决方案**: 在调用 `send` 时使用 `MSG_NOSIGNAL` 标志，可以告诉内核：“如果管道破裂（对方关闭），请不要发送 `SIGPIPE` 信号，而是让 `send` 函数调用失败并返回-1，同时将 `errno` 设置为 `EPIPE`”。这样，你的程序就可以优雅地检查返回值来处理这个错误，而不是直接崩溃。这是推荐使用 `send` 替代 `write` 的一个强有力理由。

## static和声明与实现分开的一些小知识点

```C++
// "A.hpp"

class A{
	static int a;
	static void f();
	void g();
};
// "A.cc"
int A::a = 3; // 需要在类外声明 , 所有的类公用一个a
void A::f(){
    std::cout << "This is static " << std::endl;
}
void A::g(){
    this->a;    // right
}

```

static 没有this指针 ， 其他有this指针，静态函数可以在声明时定义，但是静态变量只能在类外定义。

函数可以不定义，但是他如果被使用到了，会去全局找一个同名函数，这是极其危险的。

一般变量自然声明的时候就有定义，静态变量可以声明和定义分开，但如果使用到就必须定义。

extern 修饰可以让编译器知道这个变量被声明了，链接器在连接两个操作这个变量的文件的时候不会失效。

static 修饰全局变量 可以将它的链接性由全局转向内部，比如一个计数器 int count 如果不修饰，那就会找到两个count

inline修饰全局变量 可以把他变成外部链接，可以完成声明定义一起。

```c++
diff :
extern int a;
int a = 3;
------
inline int a = 3;
```



一个全局的 `const` 变量**默认就是内部链接 (static) 的** 

`inline static` 可以在类内同时定义声明静态变量

使用 `extern` 作为**链接规范 (linkage specification)** 的一部分，比如 `extern "C"`

```c++
class MyClass {
public:
    // 合法但罕见：指定此成员函数使用 C 语言的链接规范
    // (没有 C++ 的名字修饰，方便被 C 代码调用)
    extern "C" void c_style_function();
};
```

被 `extern "C"` {} 包裹的代码块，将取消C++的修饰规则改为c的修饰规则

主要区别就是

```c++
c++ 允许函数重载
int print(int n); // into _print_i
int print(int n,int m); // into _print_i2 
```

在 `send` 前面加上 `::`，是在明确告诉编译器：“我要调用的是全局命名空间 (Global Namespace) 中的 `send` 函数，而不是其他任何地方（比如类内部）的同名函数。可以在一个成员变量内部调用OS底层send



## 线程池

需要几个基本元素:

``` c++
struct ThreadPool{
	vector<thread>worker_; // 内部有线程等着接受一个任务，和任务无关，他只是一个不断循环的线程，他是任务的载体。
	queue<function<void()>>task_; // 任务队列，使用function<void()>实现了类型擦除
    mutex task_mtx_; // 保护任务队列
    atomic<bool> start_; // 是否开启
    atomic<int> task_wait_; // 多少任务在等待线程
    atomic<int> active_threads_; // 活跃线程
    condition_variable task_cv_ // 通知线程的条件变量
    template <class F, class... Arg>
  	auto enqueue(F&& f, Arg&&... args) -> std::future<std::invoke_result_t<F, Arg...>>;

};  
//类型擦除的具体实现
template <class F, class... Arg>
auto ThreadPool::enqueue(F&& f, Arg&&... args)
    -> std::future<std::invoke_result_t<F, Arg...>> {
  using return_type = std::invoke_result_t<F, Arg...>;
  auto ftask = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<F>(f), std::forward<Arg>(args)...));
   /*
   auto args_tuple = std::make_tuple(std::forward<Args>(args)...);

    // 3. 创建 packaged_task，其内部的 Lambda 捕获了函数 f 和包含参数的元组
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        // [f = ..., args = ...] 是 C++14 的初始化捕获，非常适合这里
        // mutable 是因为 std::apply 可能需要移动元组中的参数
        [func = std::forward<F>(f), captured_args = std::move(args_tuple)]() mutable {
            // 4. 在工作线程中，使用 std::apply 解开元组并调用函数
            return std::apply(func, std::move(captured_args));
        }
    );
   */
    
    
  std::future<return_type> res = ftask->get_future();
    
  {
    std::unique_lock<std::mutex> lock(task_mtx);
    if (!start) {
      throw std::runtime_error("enqueue on stopped ThreadPool");
    }
    this->task.push([ftask]() { (*ftask)(); });
    wait_task_++;
  }
  task_cv.notify_one(); // 通知线程池来接受
  return res;
}

```

介绍一下异步编程：

`std::packaged_task`: **任务包装器**

- **作用**: `std::packaged_task` 是实现异步调用的关键。它像一个“信封”，把你要执行的任务（即 `std::bind` 返回的可调用对象）装了进去。
- **功能**: 当这个被包装好的任务在未来的某个工作线程中被执行时，`packaged_task` 会自动捕获其返回值（或异常），并将其安全地存入一个共享的存储空间中。
- **解耦**: 它成功地将**任务的执行**与**结果的获取**这两个动作在时间和空间上分离开来。

`std::future`: **结果凭证**

- **作用**: `std::future` 是从 `packaged_task` 中获取的“取货凭证”。`enqueue` 函数**立即**返回这个凭证给调用者，而不会等待任务执行完毕。
- **功能**: 调用者线程可以随时在 `future` 对象上调用 `.get()` 方法来等待并获取结果。如果此时工作线程已经执行完任务，`.get()` 会立刻返回结果；如果还没有，`.get()` 会阻塞调用者线程，直到结果准备好。
- **异步的体现**: 调用 `enqueue` 的线程可以拿到 `future` 后继续执行其他代码，实现了任务的异步提交。

`std::promise`、`std::packaged_task` 和 `std::async` 都是为了同一个目标：**从一个线程安全地获取另一个线程的计算结果**。但它们的侧重点不同。

- **`std::async`**: 一站式服务。你给它一个函数，它负责创建线程、执行任务，并直接返回 `std::future`。
- **`std::packaged_task`**: 一个“任务包”。它将一个函数打包，并自带一个 `future`，但它不负责执行，需要你手动把它交给某个线程。
- **`std::promise`**: 它是最原始、最灵活的组件。它只负责**结果的存储与通知**，完全不管任务本身是什么，也不管任务在哪里执行。
- **工作流程**:
  1. **创建 Promise (准备空箱子)**: `std::promise<int> p;` 这创建了一个承诺，承诺未来会有一个 `int` 类型的值。
  2. **获取 Future (拿出快递单号)**: `std::future<int> f = p.get_future();` 这个 `future` 对象就是“快递单号”，你可以把它交给需要等待结果的“客户”线程。
  3. **履行承诺 (在生产者线程中装箱并封箱)**: 在某个线程（生产者）中，当你计算出最终结果后，你通过 `promise` 对象来“履行承诺”。
     - **设置值**: `p.set_value(42);`
     - **或设置异常**: 如果计算过程中发生错误，你也可以“履行一个坏的承诺”：`p.set_exception(std::make_exception_ptr(std::runtime_error("Calculation failed")));`
  4. **等待结果 (在消费者线程中凭单号取货)**: 在另一个线程（消费者）中，通过 `future` 对象等待结果。
     - `int result = f.get();`
     - `get()` 会阻塞，直到生产者调用了 `set_value` 或 `set_exception`。
     - 如果设置的是值，`get()` 返回该值。
     - 如果设置的是异常，`get()` 会将那个异常重新抛出。

`template <class F, class... Arg>`: **函数模板与可变参数模板**

- **作用**: 这是泛型编程的基础。`class F` 意味着 `f` 可以是任何可调用类型（函数指针、Lambda、函数对象等）。`class... Arg` (参数包) 意味着 `args` 可以是零个或多个任意类型的参数。

`std::invoke_result_t<F, Arg...>` 

- **作用**: 这是一个**类型萃取 (Type Trait)**，是模板元编程的核心工具。它在**编译时**回答一个问题：“如果我用 `Arg...` 这些类型的参数去调用 `F` 类型的函数，那么返回值的类型会是什么？”
- **功能**: 它让 `std::future` 的模板参数 `T` (`std::future<T>`) 能够被精确推导出来，从而使整个函数可以正确地处理任何返回类型的任务。

`F&& f, Arg&&... args` 和 `std::forward`: **完美转发 (Perfect Forwarding)**

- **作用**: `&&` 在这里不叫右值引用，而叫**转发引用 (Forwarding Reference)**。它能接收任何值类型（左值、右值、`const` 等）。
- **功能**: 配合 `std::forward`，它能将传入 `enqueue` 的参数，以其**原始的值类别**（是左值还是右值）“完美”地传递给下一层函数（在这里是 `std::bind`）。这可以避免不必要的拷贝，并支持移动语义，是现代 C++ 高性能泛型编程的关键。

### 细节实现

注意我传递给他的是一个shared_ptr指针，因为他的生命周期会比我这个函数长。

```c++
ThreadPool::ThreadPool(int n_worker) : active_thread_(0), wait_task_(0) {
  start = true;
  worker_.reserve(n_worker);
  for (int i = 0; i < n_worker; ++i) {
    worker_.emplace_back([this, i] {
      std::cout << "This is " << i << " worker" << std::endl;
      std::function<void()> f;//类型擦除对象
      while (true) {
        {
          std::unique_lock<std::mutex> lock(task_mtx);
          task_cv.wait(
              lock, [this] { return this->wait_task_ != 0 || start == false; });
          if (start == false) {
            break;
          }
          // 注意是move而不是拷贝
          f = std::move(this->task.front());
          task.pop();
          wait_task_--;
        }
        active_thread_++;
        try {
          f(); // 执行f(); 也是由于future所以 这个都不需要返回这个只需要设置值。
        } catch (const std::exception& e) {
          std::cerr << "[ThreadPool] Task execution failed: " << e.what()
                    << std::endl;
        } catch (...) {
          std::cerr
              << "[ThreadPool] Task execution failed with unknown exception"
              << std::endl;
        }
        active_thread_--;
      }
    });
  }
}
ThreadPool::~ThreadPool() {
  stop();
}
void ThreadPool::stop() {
  {
    std::unique_lock<std::mutex> lock(task_mtx);
    start = false;
  }
  task_cv.notify_all();
  for (std::thread& worker : worker_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

```



## 并发编程的坑和知识

**1. **atomic 没有默认初始化，不是一个POD，这会导致ub，在类处使用列表初始化。static修饰的会是为0

**2.** bind的使用不如lambda轻巧，也无法重载模板函数

**3.** mutable 可以解决const类型的修改问题，默认按值传递的是const类型，无法在内部修改他的值，我们move给apply需要修改，mutable就是一个不改变逻辑的承诺。

**4.** `this_thread` 是 C++11 标准库中 `<thread>` 头文件里的一个**命名空间 (namespace)**。

它的核心作用是提供一系列**独立函数**，用来访问和操作**当前正在执行的那个线程**。

`std::this_thread::get_id()` **作用**: 获取当前线程的唯一标识符 (`std::thread::id`)。

`std::this_thread::sleep_for()`

`std::this_thread::sleep_until()`

```c++
auto next_task_time = steady_clock::now() + std::chrono::seconds(5);

    std::cout << "将在5秒后执行任务..." << std::endl;
    std::this_thread::sleep_until(next_task_time);
```



## RPC框架具体介绍

### **公用的关键部分** 

```c++
TcpConnection::TcpConnection(int fd) : fd_(fd), connected_(true) {} // 一个基本的TCP连接的封装 , 一般使用shared_ptr包裹，结束时自动析构。
// 全双工，需要根据上下文判断是客户端发还是客户端送
bool TcpConnection::send(const std::string& data); // 先送前缀长度，再送文本
std::string receive(); // 收到一条消息
```

### RpcServer

#### 主要作用

响应客户端发来的rpc请求，向注册中心报告自己的心跳以及注册服务。

所有和注册中心的交互(服务注册和心跳包的发送)本质都是新建一个TCPClient 之后向注册中心发送信息。

新建一个线程用于sleep等待注册中心启动，而不是直接使用主线程sleep，防止影响其他操作（比如epoll监听到listen)。

#### **服务器组件**

```c++
class RpcServer {
public:
    using ServiceHandler = std::function<std::string(const std::string&)>;
    
    // 配置结构体 - 移到类的最前面
    struct Config {
        size_t thread_pool_size = std::thread::hardware_concurrency();
        size_t max_connections = 10000;
        int epoll_timeout = 1000; // ms
    };

    // 注册服务方法
    void registerService(const std::string& service_name, 
                        const std::string& method_name,
                        ServiceHandler handler);
    

private:
    // 服务器组件
    std::unique_ptr<EpollServer> epoll_server_;
    
    // 配置信息
    std::string registry_host_;
    int registry_port_;
    int server_port_;
    Config config_;
    
    // 服务处理器
    std::unordered_map<std::string, ServiceHandler> handlers_; // 保存服务名对应的回调函数，当RPC请求到来时，使用对应的回调函数。
    
    // 线程和状态
    std::thread heartbeat_thread_;// 心跳线程
    std::atomic<bool> running_;
      
    // 私有方法
    void handleRpcRequest(int client_fd, const std::string& data);
    void sendErrorResponse(int client_fd, const std::string& request_id, 
                          int error_code, const std::string& error_msg);
    
    void registerToRegistry();
    void registerSingleService(const std::string& service_name);
    
    void sendHeartbeat();
    void sendHeartbeatForService(const std::string& service_name);
    
    std::string getLocalIP();
};
```

##### **handleRpcRequest**（关键句柄，管理RPC请求)

接受一个发送请求的客户端**fd**和他发来的数据，这个高度模板化的请求-响应句柄会被设置为epoll解析出RPC信息之后的回调函数。

```protobuf
// RPC请求消息 携带服务名 方法名字 请求的序列化数据和独属于客户端的序号(uuid?)
message RpcRequest {
    string service_name = 1;
    string method_name = 2;
    bytes request_data = 3;
    string request_id = 4;
}

// RPC响应消息 独属于客户端的序号，错误代码，错误消息，返回的数据
message RpcResponse {
    string request_id = 1;
    int32 error_code = 2;
    string error_msg = 3;
    bytes response_data = 4;
}
```



```c++
{	
	// 设置RPC请求处理器
    epoll_server_->setConnectionHandler([this](int client_fd, const std::string& data) {
        handleRpcRequest(client_fd, data);
    });
    
    // 设置连接关闭处理器
    epoll_server_->setConnectionCloseHandler([this](int client_fd) {
        std::cout << "[RpcServer] Client disconnected (fd=" << client_fd << ")" << std::endl;
    });
}
void RpcServer::handleRpcRequest(int client_fd, const std::string& data) {
    ++total_requests_;
    try {
        // 反序列化RPC请求
        rpc::RpcRequest request;
        if (!Serializer::deserialize(data, &request)) {
            std::cerr << "[RpcServer] ✗ Failed to deserialize request from fd=" << client_fd << std::endl;
            ++failed_requests_;
            sendErrorResponse(client_fd, "", -1, "Failed to deserialize request");
            return;
        }
        
        auto start_time = std::chrono::steady_clock::now();
        // 构造响应对象
        rpc::RpcResponse response;
        response.set_request_id(request.request_id());
        
        // 查找服务处理器
        std::string method_key = request.service_name() + "." + request.method_name();
        auto handler_it = handlers_.find(method_key);
        
        if (handler_it != handlers_.end()) {
            try {
                // 调用服务处理器
                std::string result = handler_it->second(request.request_data());
                
                // 设置成功响应
                response.set_error_code(0);
                response.set_response_data(result);
                
            } catch (const std::exception& e) {
                // 处理器执行异常
                response.set_error_code(-2);
                response.set_error_msg("Handler execution failed: " + std::string(e.what()));
                std::cerr << "[RpcServer] ✗ Handler execution failed: " << e.what() << std::endl;
                ++failed_requests_;
            }
        } else {
            // 方法未找到
            response.set_error_code(-1);
            response.set_error_msg("Method not found: " + method_key);
            ++failed_requests_;
        }
        
        // 发送响应
        std::string response_data = Serializer::serialize(response);
        if (!epoll_server_->sendResponse(client_fd, response_data)) {
            ++failed_requests_;
        } else {
            std::cout << "[RpcServer] ✓ Response sent to fd=" << client_fd 
                     << " (size=" << response_data.size() << " bytes)" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[RpcServer] ✗ Request processing failed: " << e.what() << std::endl;
        ++failed_requests_;
        sendErrorResponse(client_fd, "", -3, "Internal server error: " + std::string(e.what()));
    }
}
// 简洁的错误打包返回函数
void RpcServer::sendErrorResponse(int client_fd, const std::string& request_id, 
                                 int error_code, const std::string& error_msg) {
    try {
        rpc::RpcResponse response;
        response.set_request_id(request_id);
        response.set_error_code(error_code);
        response.set_error_msg(error_msg);
        
        std::string response_data = Serializer::serialize(response);
        epoll_server_->sendResponse(client_fd, response_data);
    } catch (...) {
        // 忽略发送错误响应时的异常
    }
}
```



##### **registerService** 

注册服务也是一次性注册所有信息，其实就是向注册中心发送方法和地址和域名。

```c++
void RpcServer::registerToRegistry() {
    try {
        // 注册EchoService
        registerSingleService("EchoService");
        // 短暂延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        // 注册CalculatorService
        registerSingleService("CalculatorService");
        
    } catch (const std::exception& e) {
        std::cerr << "[RpcServer] ✗ Service registration failed: " << e.what() << std::endl;
    }
}
void RpcServer::registerSingleService(const std::string& service_name) {
    try {
        TcpClient client;
        if (!client.connect(registry_host_, registry_port_)) {
            throw std::runtime_error("Failed to connect to registry for " + service_name);
        }
        
        std::cout << "[RpcServer] ✓ Connected to registry for " << service_name << std::endl;
        
        // 创建注册请求
        rpc::ServiceRegisterRequest request;
        request.set_service_name(service_name);
        request.set_host("127.0.0.1");
        request.set_port(server_port_);
        
        std::cout << "[RpcServer] Registering " << service_name 
                  << " at 127.0.0.1:" << server_port_ << std::endl;
        
        std::string request_data = Serializer::serialize(request);
        if (!client.send(request_data)) {
            throw std::runtime_error("Failed to send registration request for " + service_name);
        }
        
        // 接收注册响应
        std::string response_data = client.receive();
        if (response_data.empty()) {
            throw std::runtime_error("Empty response from registry for " + service_name);
        }
        
        rpc::ServiceRegisterResponse response;
        if (!Serializer::deserialize(response_data, &response)) {
            throw std::runtime_error("Failed to deserialize registration response for " + service_name);
        }
        
        if (response.success()) {
            std::cout << "[RpcServer] ✓ " << service_name << " registration: " 
                     << response.message() << std::endl;
        } else {
            std::cout << "[RpcServer] ✗ " << service_name << " registration failed: " 
                     << response.message() << std::endl;
        }
        
        client.disconnect();
        
    } catch (const std::exception& e) {
        std::cerr << "[RpcServer] ✗ Failed to register " << service_name << ": " << e.what() << std::endl;
        throw;
    }
}
```

```c++
struct Config {
        size_t thread_pool_size = std::thread::hardware_concurrency();
        size_t max_connections = 10000; // epoll最大连接数
        int epoll_timeout = 1000; // ms 
};
```

**ServiceHandler** ：接受一个请求，返回对应的响应。是响应RPC请求的回调函数的基本格式。

```c++
 using ServiceHandler = std::function<std::string(const std::string&)>;
 //  在example中直接传递函数指针进去即可。 
```

一个服务方法对应的 名字是 service_name.method_name 

注册结束后，启动心跳线程，其定期为每一个服务对注册中心发送心跳包。

```c++
void RpcServer::sendHeartbeat() {
    if (!running_) return;
    
    try {
        // 为每个已注册的服务发送心跳
        sendHeartbeatForService("EchoService");
        sendHeartbeatForService("CalculatorService");  
    } catch (const std::exception& e) {
        std::cerr << "[RpcServer] ✗ Heartbeat failed: " << e.what() << std::endl;
    }
}

void RpcServer::sendHeartbeatForService(const std::string& service_name) {
    try {
        TcpClient client;
        if (!client.connect(registry_host_, registry_port_)) {
            std::cerr << "[RpcServer] Failed to connect to registry for heartbeat" << std::endl;
            return;
        }
        rpc::HeartbeatRequest request;
        request.set_service_name(service_name);
        request.set_host("127.0.0.1");
        request.set_port(server_port_);
        
        std::string request_data = Serializer::serialize(request);
        if (!client.send(request_data)) {
            std::cerr << "[RpcServer] Failed to send heartbeat for " << service_name << std::endl;
            return;
        }
        
        // 接收心跳响应
        std::string response_data = client.receive();
        rpc::HeartbeatResponse response;
        if (!response_data.empty() && Serializer::deserialize(response_data, &response)) {
            if (response.success()) {
                std::cout << service_name << std::endl;
            } else {
                std::cout <<  service_name << ": " << response.message() << std::endl;
            }
        } else {
            std::cout << "[RpcServer] ⚠ No heartbeat response for " << service_name << std::endl;
        }
        client.disconnect();
    } catch (const std::exception& e) {
        std::cerr << "[RpcServer] ✗ Heartbeat error for " << service_name << ": " << e.what() << std::endl;
    }
}
```

##### EpollServer

我们上面已经实现了EpollServer和ThreadPool的简化版，具体见上

我们来走一遍流程：

先启动listen_fd ，然后把他设置为非阻塞,bind->listen之后，将listen_fd在epoll处注册读事件(LT）。

accept 之后把客户端socket设为非阻塞，注册读事件(ET)到Epoll中，同时会把这个fd封装为一个结构体方便管理。

###### ClientConnection

```c++
struct ClientConnection {
    int fd;
    std::string receive_buffer; // 对于这个客户端发送过来的缓冲区
    std::queue<std::string> send_queue; // 发送消息缓冲队列
    std::mutex send_mutex;
    std::chrono::steady_clock::time_point last_activity;// 最后一次活跃时间
    bool closed = false;

    explicit ClientConnection(int f)
        : fd(f), last_activity(std::chrono::steady_clock::now()) {}
  };
  // 连接管理
  std::unordered_map<int, std::shared_ptr<ClientConnection>> connections_; 
```

在 **Epoll_wait ** 返回的时候根据不同的返回类型调用不同的回调函数

```c++
if (fd == listen_fd_) {
        // 新连接
        if (event.events & EPOLLIN) {
          handleNewConnection();
        }
} else {
        // 客户端事件
        if (event.events & EPOLLIN) {
          // 调用注册的回调函数
          handleClientData(fd);
        } else if (event.events & EPOLLOUT) {
          // 暂时没有这类事件
          handleClientWrite(fd);
        } else if (event.events & (EPOLLHUP | EPOLLERR)) {
          closeConnection(fd);
        }
}
```

介绍一下各个回调：

###### EpollServer::handleNewConnection

```c++
// 一个固定处理客户端到达的回调函数
void EpollServer::handleNewConnection() {
  while (true) {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(listen_fd_, (sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;  // 没有更多连接
      }
      break;
    }

    // 检查连接数限制
    if (active_connections_.load() >= config_.max_connections) {
      close(client_fd);
      continue;
    }

    // 设置客户端socket非阻塞
    if (!setNonBlocking(client_fd)) {
      close(client_fd);
      continue;
    }

    // 添加到epoll
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  // 边缘触发
    ev.data.fd = client_fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
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
  }
}
```



###### EpollServer::handleClientData(int client_fd)

```c++
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
    conn->last_activity = std::chrono::steady_clock::now()
    // 添加到接收缓冲区
    conn->receive_buffer.append(buffer, bytes_read);
    // 尝试解析完整消息,这个方法会尝试从缓冲区中循环读取 长度-数据 然后返回vector<string>
      
    auto messages = extractMessages(conn);
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
```

###### EpollServer::handleClientData(int client_fd)

```c++
void EpollServer::handleClientData(int client_fd) {
  // 首先获得实例连接
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
    // 每收到一个字节都会累加，然后更新最后一次心跳。
    bytes_received_ += bytes_read;
    conn->last_activity = std::chrono::steady_clock::now();

    // 添加到接收缓冲区
    conn->receive_buffer.append(buffer, bytes_read);

    // 尝试解析完整消息
    auto messages = extractMessages(conn, "");

    // 处理每个完整消息
    for (const auto& message : messages) {
      ++total_requests_;
	 // connection_handler_ 在RpcServer中传递为管理Rpc请求的句柄。一条message就是一个rpc请求，把他加入线程池之中，计算后返回，客户端只需要调用future.get()消费就好了。
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
 
```

#### 总结

我们来说一下他们如何组合的巧思：

首先把IO事件逻辑(recv,accept)和事件逻辑解耦，IO由EPOLL通知，accept有专有的connect句柄建立连接后，添加客户端读事件到epoll中，然后如果客户端发生了读事件，我先把读到的数据放到conn自身的缓冲区之中，然后尝试解析每一个消息，然后如果得到一条完整的消息，也就是一个rpc请求，他把用lambda包裹[ RpcServer::handleRpcRequest(cllient_fd,rpc_request) //也就是收到rpc请求后的回调，会执行消息解析，匹配方法的回调函数，调用方法回调函数(同步)，发送rpc响应消息] ，而这个本身就是void，所以模板返回值是没什么用的。





### RpcClient

本客户端实现了**本地缓存**（缓存从注册中心得到的服务名字和对应的host和port）并且设置了**过期时间**，然后提供了超时重试的机会，也提供了**负载均衡**的方式。

启动流程：启动的时候会启动一个连接池用于处理连接。

如果在本地缓存检查不到服务名，就会向注册中心发送服务请求，获得一个`vector<ServerEndpoint>` 就是所有在注册中心注册了的服务端实例，然后更新本地缓存。

```c++
 struct ServiceEndpoint {
        std::string host;
        int port;
        std::chrono::steady_clock::time_point cached_time;
        
        bool isExpired(std::chrono::seconds ttl) const {
            auto now = std::chrono::steady_clock::now();
            return (now - cached_time) > ttl;
        }
    };
```

可以检查一个实例的本地缓存是否过期。

#### 我们有两种方法调用RPC

对于每一个调用我们都可以根据策略选择如何选择多个实例中的一个进行通信，可以是随机，轮询，最新收到的。

1.**call**(const std::string& service_name, const std::string& method_name,const std::string& request_data) 

同步call，内部调用CallRetry 方法多次重试，每次重试重新构造一条rpc请求发送后返回。

2.**callAsync**

简单拿promise包裹call而已。

#### 连接池

```c++
// 连接池的配置信息
struct Config {
        size_t max_connections_per_host = 10;        // 每个主机的最大连接数
        size_t min_connections_per_host = 2;         // 每个主机的最小连接数
        std::chrono::seconds connection_timeout{10}; // 连接建立超时时间
        std::chrono::seconds idle_timeout{300};      // 空闲连接超时时间（5分钟）
        std::chrono::seconds acquire_timeout{5};     // 获取连接的超时时间
};
// 主要接口
std::shared_ptr<TcpConnection> acquire(const std::string& host, int port);
void release(const std::string& host, int port, std::shared_ptr<TcpConnection> conn);
```

Connect是一个架构分明的结构

我的线程池由三层架构组成：连接池、主机池、池化池、最底层的连接元。

```c++
// 线程池持有
std::unordered_map<std::string, std::unique_ptr<HostPool>> pools_;  // 主机池映射
mutable std::shared_mutex pools_mutex_;                     // 池映射保护锁
// 是一个 host:port 到主机池的映射。客户端向每一个地址发送的连接从这里通过acquire获得一个Tcp连接

struct HostPool {
        // 空闲连接队列
        std::queue<std::shared_ptr<PooledConnection>> idle_connections;
        
        // 所有连接（包括活跃和空闲）
        std::vector<std::shared_ptr<PooledConnection>> all_connections;
        
        // 同步原语
        mutable std::mutex mutex;
        std::condition_variable condition;
        
        // 活跃连接计数
        size_t active_count = 0;
        
        // 统计方法
        size_t getTotalConnections() const { return all_connections.size(); }
        size_t getIdleConnections() const { return idle_connections.size(); }
        size_t getActiveConnections() const { return active_count; }
};
// 提供了 TcpConnect的封装，多加了一个最后一次使用的时间，用于清理连接。
struct PooledConnection {
        std::shared_ptr<TcpConnection> connection;
        std::chrono::steady_clock::time_point last_used;
        bool in_use = false;
        
        explicit PooledConnection(std::shared_ptr<TcpConnection> conn)
            : connection(std::move(conn)), last_used(std::chrono::steady_clock::now()) {}
};

   
```

调用acquire，从空闲连接队列中取出第一个有效的连接,如果不存在则新建一个对应端口的连接。如果没有获得新连接(比如连接到达新建上限)，就间断循环等待查看是否存在连接。

调用release ，返回一个用完的连接到队列之中。

还会启动一个清理线程来清理超时连接

然后客户端需要建立连接的时候向我的连接池阻塞申请一个TCP连接

```c++	
auto conn = connection_pool_->acquire(endpoint.host, endpoint.port);
```

```c++
std::shared_ptr<TcpConnection> ConnectionPool::createConnection(const std::string& host, int port) {
    try {
        auto start_time = std::chrono::steady_clock::now();
        
        // 创建socket
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            return nullptr;
        }
        // 设置连接超时
        struct timeval timeout;
        timeout.tv_sec = config_.connection_timeout.count();
        timeout.tv_usec = 0;
        // 如果recv时间超过一个时间点直接返回一个错误。
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            std::cerr << "[ConnectionPool] Failed to set receive timeout: " << strerror(errno) << std::endl;
        }
        // 如果send时间超过一个时间点直接返回一个错误。
        if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
            std::cerr << "[ConnectionPool] Failed to set send timeout: " << strerror(errno) << std::endl;
        }
        
        // 设置TCP_NODELAY以减少延迟
        int flag = 1;
        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
            std::cerr << "[ConnectionPool] Failed to set TCP_NODELAY: " << strerror(errno) << std::endl;
        }
        
        // 设置地址
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        
        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
            close(fd);
            return nullptr;
        }
        // 连接
        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(fd);
            return nullptr;
        }
        auto conn = std::make_shared<TcpConnection>(fd);
        return conn;
        
    } catch (const std::exception& e) {
        std::cerr << "[ConnectionPool] Exception creating connection to " << host << ":" << port 
                 << ": " << e.what() << std::endl;
        return nullptr;
    }
}
// 主要是通过conn->isConnected() 来判断是否是合法连接，因为我超时的时候会设置一个字段表示连接断开。
bool ConnectionPool::isConnectionValid(const std::shared_ptr<TcpConnection>& conn) {
    if (!conn || !conn->isConnected()) {
        return false;
    }
    int error = 0;
    socklen_t len = sizeof(error);
    int fd = conn->getFd();
    
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        return false;
    }
    return error == 0;
}
// 从all_connections中移除
void ConnectionPool::removeConnection(HostPool* pool, std::shared_ptr<PooledConnection> pooled_conn) {
    auto it = std::find_if(pool->all_connections.begin(), pool->all_connections.end(),
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

std::shared_ptr<TcpConnection> ConnectionPool::acquire(const std::string& host, int port) {
    if (!running_) {
        throw std::runtime_error("Connection pool is stopped");
    }
    
    std::string key = makeKey(host, port);
    HostPool* pool = getOrCreateHostPool(key);
    
    std::unique_lock<std::mutex> lock(pool->mutex);
    auto timeout_time = std::chrono::steady_clock::now() + config_.acquire_timeout;
    
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
                return pooled_conn->connection;
            } else {
                // 移除无效连接
                removeConnection(pool, pooled_conn);
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
                return new_conn;
            } else {
                lock.lock();
            }
        }
        
        // 3. 等待连接可用或超时，如果
        if (std::chrono::steady_clock::now() >= timeout_time) {
            throw std::runtime_error("Failed to acquire connection to " + host + ":" + std::to_string(port) + " - timeout");
        }
        auto wait_until = std::min(timeout_time, std::chrono::steady_clock::now() + std::chrono::milliseconds(100));
        pool->condition.wait_until(lock, wait_until);
    }
    throw std::runtime_error("Connection pool is shutting down");
}
void ConnectionPool::release(const std::string& host, int port, std::shared_ptr<TcpConnection> conn) {
    if (!running_ || !conn) return;
    
    std::string key = makeKey(host, port);
    
    std::shared_lock<std::shared_mutex> shared_lock(pools_mutex_);
    auto pool_it = pools_.find(key);
    if (pool_it == pools_.end()) {
        conn->close();
        return;
    }
    
    HostPool* pool = pool_it->second.get();
    shared_lock.unlock();
    
    std::unique_lock<std::mutex> lock(pool->mutex);
    
    // 找到对应的连接
    auto it = std::find_if(pool->all_connections.begin(), pool->all_connections.end(),
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
            	// 放回空闲连接队列，然后通知acquire事件苏醒。
                pool->idle_connections.push(pooled_conn);
                // 通知等待的线程
                pool->condition.notify_one();
            } else {
                // 移除无效连接
                removeConnection(pool, pooled_conn);
            }
        } 
    } else {
        std::cout << "[ConnectionPool] ⚠ Connection not found in pool for " << host << ":" << port << std::endl;
        conn->close();
    }
}
```

**Nagle 算法**：这是 TCP 协议的一个默认优化。它的本意是好的：为了避免网络中充斥着大量的小数据包（比如你每按一个键就发一个包），它会把短时间内产生的多个小数据包“攒起来”，合并成一个大包再发出去。这提高了网络的吞吐量，但代价是**增加了延迟** 设置TCP_NODELAY

Shared_mutex

持有写锁还是读锁取决于你用什么类型声明她

```c++
std::shared_lock<std::shared_mutex> shared_lock(pools_mutex_);
std::unique_lock<std::shared_mutex> unique_lock(pools_mutex_);
```



```c++
	// 先加读锁
	{
        std::shared_lock<std::shared_mutex> shared_lock(pools_mutex_);
        auto it = pools_.find(key);
        if (it != pools_.end()) {
            return it->second.get();
        }
    }
    // 再加写锁
    std::unique_lock<std::shared_mutex> unique_lock(pools_mutex_);
    auto it = pools_.find(key);
    if (it != pools_.end()) {
        return it->second.get();
    }
```



### RpcRegistry

#### 主要作用

**客户端**向注册中心 发**服务发现请求**，查询一个服务全部的提供者的实例。

```protobuf
message ServiceDiscoveryRequest {
    string service_name = 1;
}
```

注册中心服务 返回**服务发现回复**，返回若干个**ServiceInstance**的实例

```protobuf
message ServiceInfo {
    string host = 1;
    int32 port = 2;
    int64 last_heartbeat = 3;
}

message ServiceDiscoveryResponse {
    repeated ServiceInfo services = 1;
}
```

本地接受的结构体定义和pb一致。

```c++
struct ServerInstance{
    std::string host;
    int port;
    std::chrono::steady_clock::time_point last_heartbeat;
};
```

**服务端**上线的时候自动向注册中心发送服务注册消息，向注册中心报告自己的服务名字，地址端口。

注册中心回复是否注册成功，以及附加消息。

```protobuf
message ServiceRegisterRequest {
    string service_name = 1;
    string host = 2;
    int32 port = 3;
}

message ServiceRegisterResponse {
    bool success = 1;
    string message = 2;
}

```

#### 主要结构体

```c++
class RegistryServer{
  	Tcpserver server_; //一个简单的应答TCP，没有使用IO多路复用，单个连接由单个线程负责。  
    std::unordered_map<std::string, std::vector<ServiceInstance>> services_;// 服务名对应的若干注册实例，返回给客户端
    std::mutex services_mutex_;// 上面的保护锁
    std::thread heartbeat_checker_; // 心跳线程，定时向所有注册的服务发送心跳包，重置ServiceInstance的最后一次心跳
    bool running_;// 是否开启
};
```

#### 主要执行句柄

```c++
void handleConnection(std::shared_ptr<TcpConnection> conn);
void handleServiceRegister(const rpc::ServiceRegisterRequest& request,rpc::ServiceRegisterResponse& response);
void handleServiceDiscovery(const rpc::ServiceDiscoveryRequest& request,rpc::ServiceDiscoveryResponse& response);
void handleHeartbeat(const rpc::HeartbeatRequest& request,rpc::HeartbeatResponse& response);
void checkHeartbeat();
```

**handleConnection** : 管理TCP服务器收到连接之后的回调函数

在内部分别尝试解析为**ServiceRegisterRequest**，**ServiceDiscoveryRequest**，**HeartbeatRequest**，三种不同的消息，如果匹配了，就交给对应的handle去执行。

**ServiceRegisterRequest**:接受一个ServerInstance，如果这个实例已经被记录了，那就更新他的心跳。

**ServiceDiscoveryRequest**：将服务名对应的vector<ServerInstance>打包发送给客户端。

**HeartbeatRequest**：更新实例的心跳。

**checkHeartbeat()**： 在服务器启动的时候，自动启动一个线程定期发送任务，任务逻辑是对于每一个服务实例检查超时时间，然后清除无效服务，服务端自动会发送心跳包。

```c++
void RegistryServer::checkHeartbeat() {
    std::lock_guard<std::mutex> lock(services_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(30); // 30秒超时
        
    for (auto& service_pair : services_) {
        const std::string& service_name = service_pair.first;
        auto& instances = service_pair.second; // 服务名对应的所有实例
        // 将无效数据移到后面，然后返回第一个不合法指针，和unique 差不多
        instances.erase(
            std::remove_if(instances.begin(), instances.end(),
                [now, timeout, &service_name](const ServiceInstance& instance) {
                    auto time_since_heartbeat = now - instance.last_heartbeat;
                    bool expired = time_since_heartbeat > timeout;
                    return expired;
                }),
            instances.end()
        );   
    }
    
    // 移除没有实例的服务
    auto service_it = services_.begin();
    while (service_it != services_.end()) {
        if (service_it->second.empty()) {
            service_it = services_.erase(service_it);
        } else {
            ++service_it;
        }
    }
}
```

# 项目QA

## Q:为什么不每个消息新建一个线程?

1.巨大的资源开销 (Resource Overhead)

- **创建与销毁成本**: 线程的创建和销毁是操作系统级别的重量级操作，它需要分配和回收内核资源、栈内存等，这个过程本身就有不可忽视的开销。对于每秒成百上千的请求，频繁创建销毁线程会消耗大量CPU时间。
- **内存成本**: 每个线程都需要自己的栈空间（在Linux上通常默认为几MB）。如果一瞬间来了1000个请求，系统就要尝试创建1000个线程，这可能会瞬间消耗掉几个GB的内存，很容易导致系统资源耗尽。

2. 灾难性的上下文切换 (Context Switching Thrashing)

这是最致命的问题。

- **什么是上下文切换**: CPU在从一个线程切换到另一个线程去执行时，需要保存当前线程的所有状态（寄存器、程序计数器等），并加载新线程的状态。这个过程是有成本的。
- **问题所在**: 现代CPU的核心数是有限的（比如8核、16核）。当你有成千上万个活跃线程时，远远超过了CPU核心数，操作系统调度器就不得不花费大量的时间在这些线程之间来回切换，以保证“雨露均沾”。
- **结果**: CPU大部分时间都浪费在了“切换”这个动作上，而不是真正地执行业务逻辑代码。当线程数达到一个阈值后，系统的总吞吐量非但不会上升，反而会**急剧下降**。这种现象被称为**系统颠簸（Thrashing）**。

线程池的优势所在

线程池正是为了规避以上两个问题而设计的：

- **线程复用**: 它预先创建了少量（通常与CPU核心数匹配）的线程，这些线程一直存在，处理完一个任务后立刻去处理下一个，完全避免了创建和销毁的开销。
- **最小化上下文切换**: 因为活跃线程数被严格控制在一个“最优”的数量级，CPU可以专注于高效地执行任务，而不是在无尽的线程间疲于奔命。

## Q:EpollServer 是如何加快服务端响应请求的?

我所构建的 `EpollServer` 架构，其精妙之处在于它在两个关键层面，通过两种不同的方式，实现了彻底的异步化，从而构建了一个分工明确、协作高效的高性能系统。

**第一层，是“前台”的I/O异步。** 我的服务器前端，凭借 **`epoll` I/O多路复用** 与 **非阻塞I/O** 的黄金组合，实现了网络层面的异步。`epoll` 扮演着高效的事件通知者，让单个线程有能力同时“监视”成千上万的网络连接而无需轮询；非阻塞I/O则保证了无论是 `accept` 新连接还是 `recv` 读数据，线程都不会被任何一个单独的操作所“卡死”。这使得我的“前台”I/O线程能够以极高的响应速度和吞吐能力，专职处理所有网络事件的接收和分发。

**第二层，是“后台”的业务处理异步。** 我通过引入**后台线程池**，并利用一个巧妙的机制，实现了I/O逻辑与业务逻辑的异步解耦。这个机制的核心，正如我所理解的——**“我（I/O线程）不需要管理（业务逻辑）何时返回”**。当前台I/O线程将一个完整的请求打包成任务并 `enqueue` 给线程池时，它的职责便已完成。它无需等待业务逻辑的漫长计算，更不关心其执行结果。这种“发射后不管”的异步“交接棒”，将I/O线程从耗时的业务处理中彻底解放出来，让它能立刻回头去服务下一个网络事件。

**这两层异步机制最终珠联璧合，形成了一条高效的“流水线”**：前台的 `epoll` 如同永不疲倦的传送带，源源不断地送来网络请求；后台的线程池则像是多个并行的精加工工位，专心致志地处理这些请求。这种设计不仅让服务器能够从容应对海量并发连接（C10K问题），更通过充分利用多核CPU的计算能力，实现了业务处理的高吞吐量，最终铸就了一个健壮、高效且具备极强伸缩性的现代化网络服务。

## Q:将监听套接字设置为非阻塞，并使用 LT 而不是 ET 为什么？

目前采用的模式：

**非阻塞 `listen_fd_`**：这意味着当您调用 `accept()` 时，如果内核的已完成连接队列中没有等待处理的连接，`accept()` 不会阻塞，而是会立刻返回-1，并设置 `errno` 为 `EAGAIN` 或 `EWOULDBLOCK`。

**`epoll` 的 LT 模式**：对于监听套接字，电平触发（LT）模式的含义是：**只要**内核的已完成连接队列中**至少还有一个**待处理的连接，那么每次调用 `epoll_wait()` 都会**立刻返回**，并报告 `listen_fd_` 上有 `EPOLLIN` 事件。它会持续通知你，直到你把队列中的所有连接都 `accept()` 完毕为止。

首先如果处理不当，会出现accept丢失问题;

还有一个在我这没体现的问题，这在多线程(Reactor)下会存在**惊群效应**，如果多个线程都在 `epoll_wait` 同一个 `epoll_fd`，一个新连接的到来（ET事件）可能会唤醒所有这些线程。这个场景一般出现在：

1、程序中**只有一个** `epoll` 实例（一个 `epoll_fd`）。

2、所有的 `socket`（包括 `listen_fd` 和所有的 `client_fd`）都被**注册一次**到这个唯一的 `epoll` 实例中。

3、然后，程序创建**多个线程**，这些线程都在自己的循环里调用 `epoll_wait()`，但它们等待的是**同一个 `epoll_fd`**。

减少主线程IO读写的开销。

## Q:你这是什么网络架构模型？

**Reactor + Proactor**

#### “主从” Reactor / 单Reactor + 工作线程池 (Reactor with Thread Pool)

- **结构**:
  - **1个 主Reactor线程 (I/O线程)**
  - **M个 工作线程 (Worker/Business-logic Thread Pool)**
- **工作流程**:
  1. I/O线程（您的 `run()` 循环）专门负责 `epoll_wait` 和所有非阻塞的I/O操作。
  2. 当I/O线程完成I/O操作（比如接收到一个完整的请求数据）后，它**不会自己去处理这个请求的业务逻辑**。
  3. 相反，它会将这个请求打包成一个任务（Task），扔给**后端的线程池**。
  4. 线程池中的某个工作线程会拿到这个任务，并执行耗时的业务逻辑。

## Q:还有什么模型？

### 多线程 Reactor / 多Reactor (Multi-Threaded Reactor)

- **结构**: **N个 Reactor线程**。
- **工作流程**: 每个线程都有自己独立的事件循环 `while -> epoll_wait -> dispatch`。它们可以监听同一个 `listen_fd` 来共同处理新连接（如Nginx），或者由一个主Reactor负责接受连接，然后把连接“分发”给其他子Reactor去处理I/O（如Netty）。
- **餐厅比喻**: 一个非常大的快餐店，有好几个一模一样的点餐柜台。每个柜台的员工（一个Reactor线程）都能独立完成从点餐到出餐的全套流程。
- **优点**: 进一步提升了I/O事件的处理能力，适用于I/O本身就成为瓶颈的极端场景。

### io_uring

####  1.io_uring 解决了 epoll 的什么痛点？

在我们之前的讨论中，`epoll` 已经非常高效了，但它依然存在一些固有的“开销”：

1. **多次系统调用 (System Calls)**: 一个典型的网络读操作，即使在`epoll`模型下，也至少需要两次系统调用：
   - `epoll_wait()`: 等待I/O就绪。
   - `recv()`: 当就绪后，真正去读取数据。 在高并发、小数据包的场景下，频繁的系统调用（涉及用户态和内核态的切换）会成为一个显著的性能瓶颈。
2. **两阶段操作**: `epoll` 只是一个**就绪通知**机制。它告诉你“可以读了”，但真正的读操作还需要你的应用程序线程自己去完成。

`io_uring` 的设计目标就是为了彻底解决这两个问题。

------



#### 2. `io_uring` 的核心架构：共享环形缓冲区



`io_uring` 的魔法在于它在**用户空间**和**内核空间**之间建立了一对**共享内存的环形缓冲区（Ring Buffer）**。这使得应用程序和内核之间可以高效地、批量地传递指令和结果，而无需每次都进行系统调用。

这两个核心组件是：

1. **提交队列 (Submission Queue - SQ)**:
   - 这是一个由**应用程序写入、内核读取**的队列。
   - 应用程序把所有想让内核做的事情（比如“从这个socket读取4KB数据到这个缓冲区”、“把这段数据写入那个文件”）封装成一个个**提交队列条目 (Submission Queue Entry - SQE)**，然后放入SQ中。
2. **完成队列 (Completion Queue - CQ)**:
   - 这是一个由**内核写入、应用程序读取**的队列。
   - 内核在完成了应用程序提交的任务后，会把结果（比如“读取成功，读到了4096字节”或“写入失败，错误码xxx”）封装成一个个**完成队列条目 (Completion Queue Entry - CQE)**，放入CQ中。

------



#### 3. 工作流程

使用`io_uring`的典型流程如下：

1. **设置 (`io_uring_setup`)**: 应用程序在启动时调用一次，创建并初始化SQ和CQ这两个环形缓冲区，并获得一个`io_uring`文件描述符。
2. **构建并提交任务**: a. 应用程序从提交队列(SQ)中获取一个可用的空位（一个SQE）。这不需要系统调用，只是一个内存操作。 b. 填充这个SQE，详细描述要做什么。例如，对于一个网络接收操作，你会设置： * 操作码：`IORING_OP_RECV` * 文件描述符：`client_fd` * 缓冲区地址：`my_buffer` * 缓冲区长度：`4096` c. 应用程序可以重复 a 和 b，一次性准备**多个**不同的任务（比如同时为一个socket读，为另一个socket写）。这叫**批量提交 (Batching)**。 d. 当所有任务都准备好后，应用程序调用一次 `io_uring_submit()` (它内部会调用`io_uring_enter`系统调用)，通知内核：“我放好了一批任务，请处理”。
3. **等待并处理结果**: a. 应用程序调用 `io_uring_wait_cqe()` 或类似的函数来等待**完成队列(CQ)\**中出现一个或多个已完成的条目（CQE）。这个调用会\**阻塞**，直到内核完成了至少一个任务。 b. 当`io_uring_wait_cqe()`返回后，应用程序遍历CQ，从中取出已完成的CQE。这**不需要系统调用**，也是内存操作。 c. 每个CQE都包含了任务的结果（比如返回值、错误码）。应用程序根据这些结果，进行下一步的业务逻辑处理（比如处理接收到的数据）。 d. 应用程序**必须**在处理完CQE后，通知`io_uring`这些CQE已经被消费，以便内核可以复用这些空间。



## Q:知道Nginx吗

### Nginx的核心架构：Master-Worker 模式

1. **Master 进程 (主进程/总管)**：
   - Nginx启动时，首先会创建一个**唯一的Master进程**。
   - 这个进程以**高权限**（通常是`root`）运行。
   - 它的核心职责是：
     - 读取并解析配置文件 (`nginx.conf`)。
     - **执行特权操作**，比如**绑定并监听端口**（如80和443）。
     - 根据配置创建（`fork`）一个或多个**Worker进程**。
     - 监控Worker进程的健康状态，如果某个Worker挂了，就重新拉起一个新的。
     - 接收外部的控制信号（如`nginx -s reload`来重新加载配置，或`stop`来关闭服务）。
   - **关键点**：**Master进程本身不处理任何客户端的HTTP请求。**
2. **Worker 进程 (工作进程/工人)**：
   - 由Master进程创建，创建后会**继承**Master进程已经打开的监听套接字（listening sockets）。
   - 以**低权限**用户（如`www-data`, `nginx`）的身份运行，以增加安全性。
   - **这是真正干活的进程**。每个Worker进程都有自己独立的`epoll`事件循环，负责接收（`accept`）新连接、处理HTTP请求、与后端服务通信、发送响应数据等所有具体工作。
   - Worker进程的数量通常配置为等于服务器的**CPU核心数**，以充分利用多核CPU的性能。

### 核心问题：多个Worker进程如何共同处理一个端口？

既然所有Worker都继承了同一个监听端口，它们如何协调，避免冲突呢？这里主要有两种主流的实现机制：

#### 机制一：经典模型 —— 共享 `listen` 套接字 + `accept_mutex` (连接互斥锁)

这是Nginx早期和默认的行为。

1. **共享套接字**：所有Worker进程都持有从Master继承来的**同一个** `listen_fd`。
2. **等待事件**：所有Worker都在自己的`epoll`中等待这个共享的`listen_fd`上的新连接事件。
3. **惊群问题**: 当一个新连接到来时，内核会通知`epoll`，这可能会导致**所有**正在等待的Worker进程都被唤醒（这就是我们之前讨论的“惊群”）。
4. **`accept_mutex` 锁机制**: 为了避免所有被唤醒的Worker都去调用`accept()`造成混乱和浪费，Nginx实现了一个“连接互斥锁”。
   - 只有一个成功获取到这个锁的Worker进程，才会去调用`accept()`处理这个新连接。
   - 其他没有抢到锁的Worker进程，会发现锁已被占用，于是它们会放弃本次`accept`，重新回到`epoll_wait`中等待下一个事件。

- **餐厅比喻**：餐厅只有一个前门（共享的`listen_fd`）。当有客人上门时（新连接），所有服务员（Worker进程）都看到了。但公司规定，只有一个拿到了“接待令牌”（`accept_mutex`）的服务员才能上前去接待客人。



#### 机制二：现代模型 —— `SO_REUSEPORT` (端口复用)



这是在较新的Linux内核（3.9+）上，Nginx推荐使用的更高效的方式。需要在`nginx.conf`的`listen`指令后添加`reuseport`参数来开启。

1. **独立套接字**：在这种模式下，**每个Worker进程都会创建自己独立的 `listen_fd`**，并使用`SO_REUSEPORT`这个socket选项，将它们**绑定到同一个IP和端口**上。这在没有`SO_REUSEPORT`之前是不允许的。
2. **内核负责分发**：当一个新连接到达这个端口时，**Linux内核本身**会负责决定将这个连接交给哪个Worker进程的`listen_fd`去处理。它通常使用一个基于连接四元组（源IP、源端口、目标IP、目标端口）的哈希算法，来实现负载均衡。
3. **无锁竞争**：因为连接在到达用户态之前就已经由内核分配好了，所以Worker进程之间**不存在 `accept` 竞争**。每个Worker只处理内核分配给自己的连接，效率更高，并且负载分配通常也更均匀。

- **餐厅比喻**：餐厅开了好几个一模一样的前门（每个Worker都有独立的`listen_fd`），并且都在外面挂着同一个“XX餐厅”的招牌。当有客人来时，门口的智能引导系统（内核）会根据哪个门前人少，直接把客人引导到其中一个门口。服务员们各司其职，互不干扰。

## Q:客户端的异步？

使用std::**async**(std::launch::async, [this, service_name, method_name, request_data]() {

​    return **call**(service_name, method_name, request_data);

  })的方法简单，但是每一次都要重新新建一个线程，而且发送接受响应这个过程是紧密集合的，我们可以解耦开来，先声明一个map<uuid,promise<string>>, 用几个固定连接去发送，拿epoll去监听响应，然后获得返回的值之后，填入一开始确定uuid的promise之中，然后就结束了。这样其实本质就是建立了线程的建立和销毁，用一个固定的线程池去响应连接。

## Q:Async是什么？

async(std::launch::async, ...) 然后接受一个和thread一样的传参法则

但要注意如果传参记得传函数指针

lambda其实就是一个重载了（）方法的函数类型。

```c++
int add(int a,int b){
    return a+b;
}

thread t1(add(1,2))// wrong 这直接执行了
thread t1（&add, 1,2 ) // right
thread t1([]{
    add(1,2);
})//right 
    记得成员函数要传this这个都是一样的。
```

## **Q: 你的连接池如何应对“集中连接失效”（Connection Avalanche）问题？**



**A:** 我的连接池设计了“被动清理”与“主动隔离”相结合的双重保障机制来应对此问题。

1. **被动清理 (后台线程)**: 一个后台清理线程会定期（如每30秒）扫描并移除空闲超时的连接，这是一个常规的、保证连接池长期健康的“保底”机制。
2. **主动隔离与恢复 (核心机制)**: 当服务器集中重启导致大量连接同时失效时，依赖被动清理响应太慢。为此，我设计了一套“主动隔离”流程：
   - **触发隔离**: 在`acquire`方法中，引入一个原子计数器或时间窗口，当短时间内检测到连续N个连接获取失败时，该`HostPool`会立即被标记为一个特殊的**“紧急维护”（Quarantine）**状态。
   - **隔离期行为**: 一旦进入“紧急维护”状态，后续所有尝试从该池`acquire`连接的业务线程，都不会再徒劳地检查空闲队列。相反，它们会通过条件变量`cv.wait_for()`进入一个**带超时和随机抖动的等待**，从而把CPU资源让出来，避免无意义的空转和锁竞争。
   - **异步清理**: 触发隔离的“吹哨人”线程会负责唤醒后台清理线程，使其**立即**执行一次全量的坏连接清理工作。
   - **恢复**: 清理线程在完成工作后，会重置“紧急维护”标志，并调用`notify_one()`唤醒一个等待的业务线程，形成一个有序的“接力”恢复链。其他等待的线程则通过`wait_for`的超时机制被**打散唤醒**，避免了“惊群效应”，最终让连接池平稳、快速地从雪崩中恢复。或者每获得一个连接就`notify_one` 做一个接力连接。

------



## **Q: 你的RPC框架如何实现优雅关闭（Graceful Shutdown）？**



**A:** 我设计的优雅关闭流程是一个由服务端主动协调、客户端智能响应的精密过程，确保了数据零丢失和业务无感知。

1. **服务端侧（“跛脚鸭” + 主动通知）**:
   - **第一步：下线隔离**。收到关闭信号后，服务端**立即从注册中心反注册**，从源头切断新流量。同时，停止`accept`新的TCP连接。
   - **第二步：主动通知**。服务端会遍历所有当前已建立的TCP连接，通过Protobuf定义的**管理指令（一个特殊的RPC消息）**，主动向每个客户端推送“服务即将下线”的信号。
   - **第三步：处理存量请求**。在等待客户端响应和断开连接的同时，服务端会调用`.join()`等待其后台业务线程池处理完所有已在队列中的任务。
   - **第四步：最终超时保障**。为了防止被有问题的客户端拖累，服务端会启动一个最终超时定时器（例如，基于TCP的2MSL或一个固定的30秒）。一旦超时，无论连接是否全部断开，都会强制关闭剩余连接并退出进程。
2. **客户端侧（智能响应与安全重试）**:
   - **响应关闭信号**: 客户端有专门的逻辑来处理服务端推送的“下线”管理指令，收到后会**立即**从本地服务发现缓存中移除该服务端实例。
   - **处理并发冲突**: 设计中考虑了一个关键的竞态条件——业务线程刚决定连接服务端A，但同时后台线程收到了A的下线通知。在这种情况下，如果请求依然发往了A，服务端A会返回一个**特定业务错误码**（如 `ERR_SERVER_SHUTTING_DOWN`）。客户端收到此错误后，能100%确定请求未被执行，从而可以**安全地进行重试**，并将请求无缝切换到其他健康的服务端实例上。

------



#### **Q3: 你对中心化服务发现之外的模式有何思考？（去中心化方案）**



**A:** 是的，为了解决中心化注册中心的单点故障和性能瓶颈问题，我构思了一套基于Gossip协议的去中心化服务发现方案。

1. **冷启动（Bootstrapping）**: 客户端和服务端在启动时，会硬编码一部分**种子节点（Seed Nodes）**的地址。只要能连上任何一个种子节点，就可以获取到当前集群的部分成员列表，从而“进入”这个去中心化的网络。
2. **成员关系与信息同步 (Gossip)**:
   - 节点之间会定期、随机地选择其他节点交换信息，这种“闲聊”包含了两个核心内容：
     - **心跳与健康状态**: 用于判断哪些节点还活着（成员关系管理）。
     - **服务注册信息**: 包含了每个节点提供的服务名和地址。
   - 通过这种病毒式、指数级的传播，一个节点的状态变化（如新服务上线或节点下线）能在很短的时间内扩散到整个网络，最终达到**最终一致性（Eventual Consistency）**。
3. **风险与权衡**:
   - **代价**: 这种模式的代价是增加了节点间的**网络IO开销**。
   - **核心挑战**: 最大的挑战在于如何处理**“脑裂”（Split-brain）**问题。当网络发生分区，集群被分割成多个无法通信的子集时，需要有成熟的共识算法（如Raft, Paxos）或基于Quorum（法定人数）的机制来保证数据的一致性，避免多个子集都认为自己是唯一的主集群而导致数据错乱。这是从“高可用”迈向“高一致性”的关键一步。
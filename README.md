🚀 Cpp20 Coroutine WebServer
基于 C++20 无栈协程 (Coroutines) 与 Epoll 实现的高性能、轻量级全异步 Web 服务器。
本项目彻底摒弃了传统“半同步/半反应堆”线程池模型带来的上下文切换开销与回调地狱（Callback Hell），采用 One Loop Per Thread 架构，并利用协程原语（co_await）将底层异步 IO 封装为同步语义，单机轻松实现数万级并发连接。

✨ 核心特性 / Features
- 🌐 极简异步模型：基于 C++20 <coroutine> 底层原语 (promise_type, awaitable) 深度封装，实现无锁、无回调的纯线性异步业务逻辑。
- ⚡ 高并发架构：采用 Multi-Reactor (主从 Reactor) 架构。主线程负责 Accept，通过 Round-Robin 算法分发 fd，利用 eventfd 实现跨线程的无阻塞精确唤醒。
- 📡 Epoll 底层驱动：网络 IO 采用 Epoll 边缘触发 (ET) + 非阻塞模式，配合协程调度器，CPU 始终保持高效运转。
- 📝 HTTP/1.1 解析器：手写有限状态机 (FSM) 解析 HTTP 报文，支持 GET / POST 请求，支持 application/json 与表单数据解析，完美支持 Keep-Alive 长连接。
- 🚀 零拷贝技术：处理静态大文件资源时，采用 sendfile 系统调用结合 TCP_CORK 选项，实现 DMA 级别的 Zero-Copy 传输，CPU 拷贝开销降至 0。
- 🛡️ 高可用基础设施： 
  - 定时器：基于 std::vector 实现的 小根堆 (Min-Heap) 定时器，支持惰性与主动删除，精准剔除超时僵尸连接。
  - 数据库池：结合 C++20 <semaphore> (计数信号量) 和 RAII 机制，实现高效安全的 MySQL 数据库连接池。
  - 异步日志：基于生产者-消费者模型，结合现代 fmt 库格式化，后台独立线程双缓冲写盘，确保峰值流量下主逻辑不被阻塞。

📊 性能压测 / Benchmark
- 测试环境：Ubuntu 20.04(wsl2) R5-5600(12 Core CPU) 32GB RAM
- 测试工具：Webbench
- 压测结果：在 60 秒内进行满负荷高并发压测，成功稳定支撑 28000+ 并发连接，QPS约为 2500 且响应延迟极低，无由于资源耗尽导致的失败请求。

$ webbench -c 28000 -t 60 http://localhost:8080/
Webbench - Simple Web Benchmark 1.5
Running info: 28000 clients, running 60 sec.
Speed=140721 pages/min, 317426 bytes/sec.
Requests: 140721 susceed, 0 failed.

🛠️ 环境要求 / Requirements
- OS: Linux (推荐 Ubuntu 20.04 及以上)
- Compiler: GCC 11+ / Clang 13+ (必须完整支持 C++20 协程标准)
- Build Tool: CMake 3.10+
- Dependencies: 
  - MySQL Client (sudo apt install libmysqlclient-dev)
  - Fmt library (sudo apt install libfmt-dev)

⚙️ 编译与运行 / Build & Run
1. 初始化数据库
请在本地 MySQL 中创建对应的数据库与表：
CREATE DATABASE webserver;
USE webserver;
CREATE TABLE user(
    username char(50) NULL,
    password char(50) NULL
);
INSERT INTO user(username, password) VALUES('root', '123456');

2. 编译运行
./run_server.sh

3. 访问测试
- 浏览器访问静态主页：http://localhost:8080/
- 使用 httpie 测试 POST JSON 请求： 
http -v POST http://localhost:8080/login user=root pwd=123456

📂 目录结构 / Directory Structure
.
├── include/
│   ├── BlockQueue.h      # 异步队列
│   ├── Buffer.h          # 支持自动扩容的高性能缓冲区
│   ├── Epoll.h           # Epoll IO 多路复用封装
│   ├── EventLoop.h       # 协程事件循环调度器
│   ├── HttpRequest.h     # HTTP 状态机解析器 (支持 JSON/Form)
│   ├── HttpResponse.h    # HTTP 响应构建与 sendfile 零拷贝
│   ├── IoAwaitable.h     # C++20协程等待体
│   ├── Log.h             # 异步日志系统
│   ├── Result.h          # C++20 Task 与 promise_type 封装
│   ├── Socket.h          # RAII Socket 与 Awaitable 等待体
│   ├── SqlConnPool.h     # 基于 C++20 信号量的 MySQL 连接池
│   ├── Timer.h           # 小根堆连接超时管理器
│   ├── Utils.h           # 辅助函数
│   └── Worker.h          # 工作线程与线程池封装
├── src/                  # 具体核心源码实现
├── resources/            # 静态 web 资源目录 (HTML/JPG)
├── run_server.sh/        # 构建脚本
└── CMakeLists.txt        # CMakeLists构建

🧠 技术原理解析 / Architecture Detail
为什么选择 C++20 协程？
传统的 C++ Web 服务器（如 Nginx 源码或 TinyWebServer）多采用 状态机 + 回调函数 + 业务线程池 的模式。这种模式在处理复杂业务（如一次请求需要多次查库、多次读写）时，会导致代码逻辑被严重割裂（Callback Hell），且线程切换的开销不可忽视。
本项目利用 C++20 协程的 co_await 关键字，在网络 Socket 遇到 EAGAIN 时**主动挂起（Suspend）**当前协程，将 CPU 交还给 EventLoop 去处理其他连接；当 Epoll 监听到可读/可写事件时，再通过保存在 Map 中的 coroutine_handle 恢复（Resume）执行。
这使得我们可以在单线程内并发处理上万个连接，既拥有了同步代码的极佳可读性，又享受了异步 IO 的极限性能。

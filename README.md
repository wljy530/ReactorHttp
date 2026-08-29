# ReactorHttp

基于 **主从 Reactor 多线程模型** 的轻量级 HTTP 静态资源服务器，使用 C 语言实现，运行于 Linux。

- **主线程（主 Reactor）**：只负责 `accept` 新连接，不参与业务读写
- **线程池（从 Reactor）**：N 个子线程，每个子线程各自运行一个事件循环（`EventLoop`），负责处理被分配到的所有连接的 IO 事件
- **IO 多路复用**：默认使用 `epoll`，通过 `Dispatcher` 函数指针抽象层可无缝切换 `poll` / `select`
- **HTTP 协议**：基于状态机解析请求（请求行 → 请求头 → 请求体），支持目录列表、文件下载、按后缀识别 `Content-Type`

## 目录结构

| 文件 | 作用 |
|---|---|
| `main.c` | 程序入口，解析命令行参数（端口、资源目录），启动服务器 |
| `TcpServer.c/h` | 服务器骨架：监听 socket 初始化、`accept` 回调、启动线程池与主循环 |
| `EventLoop.c/h` | **反应堆核心**：事件循环 + 任务队列 + socketpair 本地唤醒机制 |
| `Dispatcher.h` | 多路复用抽象接口（`init/add/remove/modify/dispatch/clear` 函数指针表） |
| `EpollDispatcher.c` | epoll 实现（当前默认） |
| `PollDispatcher.c` | poll 实现 |
| `SelectDispatcher.c` | select 实现 |
| `Channel.c/h` | 文件描述符与事件、回调的绑定 |
| `ChannelMap.c/h` | fd → Channel 映射表（动态数组，按需倍增扩容） |
| `ThreadPool.c/h` | 线程池：启动子线程、轮询分配连接 |
| `WorkerThread.c/h` | 单个子线程：初始化并运行自己的 `EventLoop` |
| `TcpConnection.c/h` | 一条 TCP 连接的读/写回调与资源销毁 |
| `Buffer.c/h` | 读写缓冲区（`readPos`/`writePos`/`capacity`，自动合并与扩容） |
| `HttpRequest.c/h` | HTTP 请求解析状态机、URL 解码、目录/文件响应函数 |
| `HttpResponse.c/h` | HTTP 响应组织：状态行、响应头、响应体回调 |
| `Log.h` | 日志宏（`DEBUG` 开关控制） |

## 线程模型

```
                        主线程
                mainLoop (监听 Reactor)
           epoll 只登记 listen fd 的读事件
                     accept → 新连接 cfd
                          │
              takeWorkerEventLoop 轮询分配
        ┌─────────────────┼─────────────────┐
        ▼                 ▼                 ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│ SubThread-0  │  │ SubThread-1  │  │ SubThread-N  │
│  evLoop #0   │  │  evLoop #1   │  │  evLoop #N   │
│ epoll: cfd…  │  │ epoll: cfd…  │  │ epoll: cfd…  │
└──────────────┘  └──────────────┘  └──────────────┘
```

要点：

1. **主线程**：`eventLoopRun(mainLoop)` 阻塞在 `epoll_wait`，只关心 `listen fd`。一旦可读就执行 `acceptConnection`。
2. **连接分配**：`takeWorkerEventLoop` 以轮询（`index = (index + 1) % threadNum`）方式从线程池取出一个子线程的 `evLoop`，连接交给该子线程管理。
3. **子线程**：每个子线程在 `subThreadRunning` 中调用 `eventLoopInitEx` 创建**自己的** `EventLoop`（含独立的 epoll 实例、任务队列、socketpair），随后 `eventLoopRun` 阻塞在自己的 `epoll_wait` 上。
4. **跨线程唤醒**：主线程往子线程注册连接时，子线程可能正阻塞在 `epoll_wait`。主线程通过 `taskWakeup` 向子线程的 `socketPair[0]` 写入数据，子线程的 `socketPair[1]` 作为读事件被唤醒，随后 `eventLoopProcessTask` 处理任务队列（ADD/DELETE/MODIFY）。


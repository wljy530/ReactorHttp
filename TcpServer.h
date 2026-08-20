#pragma once
#include "EventLoop.h"
#include "ThreadPool.h"

struct Listener
{
	int lfd;              // 监听描述符
	unsigned short port;  // 端口号
};
struct TcpServer
{
	int threadNum;  // 子线程个数
	struct EventLoop* mainLoop;     // 反应堆模型
	struct ThreadPool* threadPool;  // 线程池
	struct Listener* listener;
};

// 初始化
struct TcpServer* tcpServerInit(unsigned short port, int threadNum);

// 初始化监听
struct Listener* listenerInit(unsigned short port);

// 服务器的读回调函数
int acceptConnection(void* arg);
// 启动服务器
void tcpServerRun(struct TcpServer* server);
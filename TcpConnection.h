#pragma once
#include "EventLoop.h"
#include "Buffer.h"
#include "Channel.h"

struct TcpConnection
{
	char name[32];             // 根据文件描述符起的名字
	struct EventLoop* evLoop;  // 子线程中的反应堆实例
	struct Channel* channel;   // 关于指定文件描述符(客户端)的信息 
	struct Buffer* readBuf;    // 读数据内存块
	struct Buffer* writeBuf;   // 写数据内存块
};

// 读取客户端数据的回调函数
int processRead(void* arg);
// 初始化
struct TcpConnection* tcpConnectionInit(int fd, struct EventLoop* evLoop);

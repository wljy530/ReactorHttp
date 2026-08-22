#pragma once
#include "EventLoop.h"
#include "Buffer.h"
#include "Channel.h"

// #define MSG_SEND_AUTO 
// 将准备发送的数据准备好后一起发送，如果注释上面的一行代码就是有一点数据就先发一点数据，
// 明显第二种方式可以避免申请内存不足以一次放下需要发送数据的情况
struct TcpConnection
{
	char name[32];             // 根据文件描述符起的名字
	struct EventLoop* evLoop;  // 子线程中的反应堆实例
	struct Channel* channel;   // 关于指定文件描述符(客户端)的信息 
	struct Buffer* readBuf;    // 读数据内存块
	struct Buffer* writeBuf;   // 写数据内存块
	// http协议
	struct HttpRequest* request;
	struct HttpResponse* response;
};

// 读取客户端数据的回调函数
int processRead(void* arg);
// 发送给客户端数据的回调函数
int processWrite(void* arg);
// 初始化
struct TcpConnection* tcpConnectionInit(int fd, struct EventLoop* evLoop);

// 释放内促
int tcpConnectionDestroy(void* arg);

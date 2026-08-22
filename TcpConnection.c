#include "TcpConnection.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include <stdlib.h>
#include <stdio.h>

int processRead(void* arg)
{
	struct TcpConnection* conn = (struct TcpConnection*)arg;

	// 接收数据
	int count = bufferSocketRead(conn->readBuf, conn->channel->fd);
	if (count > 0)
	{
#ifdef MSG_SEND_AUTO  // 这种一起发送数据的方式需要多路复用检测写事件后，才能一并发送数据，因此在读事件中不能断开连接
		// 增加检测写事件，使后续服务器可以给客户端发送数据
		writeEventEnable(conn->channel, true);  
		eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
#endif

		// 接收到了http请求，解析http请求
		int socket = conn->channel->fd;
		bool flag = parseHttpRequest(conn->request, conn->readBuf, conn->response, conn->writeBuf, socket);
		if (!flag)
		{
			// 解析失败，回复一个简单的html
			char* errMsg = "Http/1.1 400 Bad Request\r\n\r\n";  // C语言的字符串字面量会隐式添加'\0'
			bufferAppendString(conn->writeBuf, errMsg);
		}
	}

#ifndef MSG_SEND_AUTO
	// 断开连接
	eventLoopAddTask(conn->evLoop, conn->channel, DELETE);  // 向任务队列中加入删除任务
#endif

	return 0;
}

int processWrite(void* arg)
{
	struct TcpConnection* conn = (struct TcpConnection*)arg;

	// 发送数据
	int count = bufferSendData(conn->writeBuf, conn->channel->fd);
	if (count > 0)
	{
		// 判断数据是否被完全发送出去了
		if (bufferReadableSize(conn->writeBuf) == 0)
		{
			// 和服务器断开连接 -- 删除这个节点
			eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
		}
	}

	return 0;
}

struct TcpConnection* tcpConnectionInit(int fd, struct EventLoop* evLoop)
{
	struct TcpConnection* conn = (struct TcpConnection*)malloc(sizeof(struct TcpConnection));
	sprintf(conn->name, "Connection-%d", fd);
	conn->readBuf = bufferInit(10240);
	conn->writeBuf = bufferInit(10240);
	conn->evLoop = evLoop;
	conn->channel = channelInit(fd, ReadEvent, processRead, processWrite, tcpConnectionDestroy, conn);
	eventLoopAddTask(conn->evLoop, conn->channel, ADD);  // 添加任务到子线程的任务队列中
	// http
	conn->request = httpRequestInit();
	conn->response = httpResponseInit();

	return conn;
}

int tcpConnectionDestroy(void* arg)
{
	struct TcpConnection* conn = (struct TcpConnection*)arg;
	if (conn != NULL)
	{
		if (conn->readBuf && bufferReadableSize(conn->readBuf) == 0 && 
			conn->writeBuf && bufferWriteableSize(conn->writeBuf) == 0)
		{
			// 成员变量中的evLoop不应该释放，因为那是属于子线程的
			destoryChannel(conn->evLoop, conn->channel);
			bufferDestroy(conn->readBuf);
			bufferDestroy(conn->writeBuf);
			httpRequestDestroy(conn->request);
			httpResponseDestroy(conn->response);
			free(conn);
		}
	}

	return 0;
}

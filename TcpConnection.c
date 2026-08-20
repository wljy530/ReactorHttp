#include "TcpConnection.h"

int processRead(void* arg)
{
	struct TcpConnection* conn = (struct TcpConnection*)arg;

	// 接收数据
	int count = bufferSocketRead(conn->readBuf, conn->channel->fd);
	if (count > 0)
	{
		// 接收到了http请求，解析http请求

	}
	else
	{
		// 断开连接

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
	conn->channel = channelInit(fd, ReadEvent, processRead, NULL, conn);
	eventLoopAddTask(conn->evLoop, conn->channel, ADD);  // 添加任务到子线程的任务队列中

	return conn;
}

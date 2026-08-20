#include "TcpServer.h"
#include <sys/types.h>         
#include <sys/socket.h>
#include <netinet/in.h>
#include "TcpConnection.h"

struct TcpServer* tcpServerInit(unsigned short port, int threadNum)
{
	struct TcpServer* tcp = (struct TcpServer*)malloc(sizeof(struct TcpServer));
	tcp->threadNum = threadNum;
	tcp->mainLoop = eventLoopInit();
	tcp->threadPool = threadPoolInit(tcp->mainLoop, threadNum);
	tcp->listener = listenerInit(port);

	return tcp;
}

struct Listener* listenerInit(unsigned short port)
{
	struct Listener* listener = (struct Listener*)malloc(sizeof(struct Listener));

	// 1. 创建监听的fd
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (lfd == -1)
	{
		perror("socket");
		return -1;
	}

	// 2. 设置端口复用
	int opt = 1;
	int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret == -1)
	{
		perror("setsockopt");
		return -1;
	}

	// 3. 绑定端口和IP
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	
	ret = bind(lfd, (struct sockaddr*)&addr, sizeof(addr));
	if (ret == -1)
	{
		perror("bind");
		return -1;
	}

	// 4. 设置监听
	ret = listen(lfd, 128);
	if (ret == -1)
	{
		perror("listen");
		return -1;
	}

	// 5. 初始化listener
	listener->lfd = lfd;
	listener->port = port;

	return listener;
}

int acceptConnection(void* arg)
{
	struct TcpServer* server = (struct TcpServer*)arg;

	// 和客户端建立连接
	int cfd = accept(server->listener->lfd, NULL, NULL);

	// 从线程池里取出一个子线程的反应堆实例去处理这个cfd
	struct EventLoop* evLoop = takeWorkerEventLoop(server->threadPool);
	// 将cfd放到TcpConnection中处理
	tcpConnectionInit(cfd, evLoop);

	return 0;
}

void tcpServerRun(struct TcpServer* server)
{
	// 启动线程池
	threadPoolRun(server->threadPool);

	// 添加检测的任务到主线程反应堆实例中的任务队列里
	struct Channel* channel = channelInit(server->listener->lfd, ReadEvent, acceptConnection, NULL, server);
	eventLoopAddTask(server->mainLoop, channel, ADD);

	// 启动反应堆模型
	eventLoopRun(server->mainLoop);
}

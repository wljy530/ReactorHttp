#include "EventLoop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "Dispatcher.h"
#include <sys/socket.h>
#include <unistd.h>

// 写数据
void taskWakeup(struct EventLoop* evLoop)
{
	const char* msg = "子线程快醒，别再阻塞了!!!";
	write(evLoop->socketPair[0], msg, strlen(msg));
}

// 读数据
int readLocalMessage(void* arg)
{
	struct EventLoop* evLoop = (struct EventLoop*)arg;
	char buf[256] = { 0 };
	read(evLoop->socketPair[1], buf, sizeof(buf));

	return 0;
}

struct EventLoop* eventLoopInit()
{
	return eventLoopInitEx(NULL);
}

struct EventLoop* eventLoopInitEx(const char* threadName)
{
	struct EventLoop* evLoop = (struct EventLoop*)malloc(sizeof(struct EventLoop));
	evLoop->isQuit = false;

	// 选择多路复用模型，这里选择epoll
	evLoop->dispatcher = &EpollDispatcher;  // 指向声明的全局变量
	evLoop->dispatcherData = evLoop->dispatcher->init();

	// 任务队列
	evLoop->head = NULL;
	evLoop->tail = NULL;

	// map
	evLoop->channelMap = channelMapInit(128);

	// 线程id，name，mutex
	evLoop->threadID = pthread_self();
	// 主线程调用该函数传参为NULL，以此来区分调用对象是主线程还是子线程
	strcpy(evLoop->pthreadName, threadName == NULL ? "MainThread" : threadName);
	pthread_mutex_init(&evLoop->mutex, NULL);

	// 本地通信fd
	int ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, evLoop->socketPair);
	if (ret == -1)
	{
		perror("sockerpair");
		exit(0);
	}
	// 指定规则: evLoop->socketPair[0] 发送数据，evLoop->socketPair[1] 接收数据
	// 因此需要把evLoop->socketPair[1]放进任务队列
	struct Channel* channel = channelInit(evLoop->socketPair[1], ReadEvent, readLocalMessage, NULL, NULL, evLoop);
	eventLoopAddTask(evLoop, channel, ADD);  // 将channel添加到任务队列

	return evLoop;
}

int eventLoopRun(struct EventLoop* evLoop)
{
	assert(evLoop != NULL);  // 断言确保evLoop指针非空

	if (evLoop->threadID != pthread_self())  // 比较当前线程id和成员变量是否一致
	{
		return -1;
	}

	// 取出事件分发和检测模型
	struct Dispatcher* dispatcher = evLoop->dispatcher;
	
	// 循环进行事件处理
	while (!evLoop->isQuit)
	{
		dispatcher->dispatch(evLoop, 2);  // 超时时长 2s
		eventLoopProcessTask(evLoop);
	}

	return 0;
}

int eventActivate(struct EventLoop* evLoop, int fd, int event)
{
	if (fd < 0 || evLoop == NULL)
	{
		return -1;
	}

	// 取出channel
	struct Channel* channel = evLoop->channelMap->list[fd];
	assert(channel->fd == fd);

	if (event & ReadEvent && channel->readCallback)
	{
		channel->readCallback(channel->arg);
	}
	if (event & WriteEvent && channel->writeCallback)
	{
		channel->writeCallback(channel->arg);
	}

	return 0;
}

int eventLoopAddTask(struct EventLoop* evLoop, struct Channel* channel, enum ElemType type)
{
	// 加锁，保护共享资源
	pthread_mutex_lock(&evLoop->mutex);

	// 创建新节点
	struct ChannelElement* node = (struct ChannelElement*)malloc(sizeof(struct ChannelElement));
	node->type = type;
	node->channel = channel;
	node->next = NULL;

	if (evLoop->head == NULL)  // 链表为空
	{
		evLoop->head = evLoop->tail = node;
	}
	else  // 链表非空
	{
		evLoop->tail->next = node;
		evLoop->tail = node;
	}

	pthread_mutex_unlock(&evLoop->mutex);

	// 处理节点(基于子线程的角度分析)
	/*
	* 细节:
	*	1. 对于链表节点的添加：可能是当前线程也可能是其他线程(主线程)，因此需要加互斥锁
	*		1). 修改fd的事件，当前子线程发起，当前子线程处理
	*		2). 添加新的fd，添加任务节点的操作是由主线程发起的
	*   2. 主线程主要负责监听，不能让主线程处理任务队列，需要由当前的子线程去处理
	*/
	if (evLoop->threadID == pthread_self())  // 当前线程为子线程
	{
		// 注意其他注释的分析全是从子线程角度分析，如果是主线程调用该函数就是走这个分支，处理任务队列中连接客户端的任务
		eventLoopProcessTask(evLoop);  
	}
	else  
	{
		// 主线程 -- 告诉子线程处理任务队列中的任务
		// 1. 子线程在工作(无影响) 2. 子线程被阻塞了:select，poll，epoll（需要解除阻塞）
		taskWakeup(evLoop);
	}

	return 0;
}

int eventLoopProcessTask(struct EventLoop* evLoop)
{
	pthread_mutex_lock(&evLoop->mutex);
	
	struct ChannelElement* head = evLoop->head;
	while (head != NULL)
	{
		struct Channel* channel = head->channel;
		if (head->type == ADD)          // 添加
		{
			eventLoopAdd(evLoop, channel);
		}
		else if (head->type == DELETE)  // 删除
		{
			eventLoopRemove(evLoop, channel);
		}
		else if (head->type == MODIFY)  // 修改
		{
			eventLoopModify(evLoop, channel);
		}

		struct ChannelElement* temp = head;
		head = head->next;
		free(temp);
	}
	evLoop->head = evLoop->tail = NULL;

	pthread_mutex_unlock(&evLoop->mutex);

	return 0;
}

int eventLoopAdd(struct EventLoop* evLoop, struct Channel* channel)
{
	struct ChannelMap* channelMap = evLoop->channelMap;
	int fd = channel->fd;
	if (fd >= channelMap->size)  // 没有足够空间存储键值对，需要扩容
	{
		if (!makeMapRoom(channelMap, fd, sizeof(struct ChannelMap*)))
		{
			return -1;
		}
	}

	int ret = -1;
	// 找到fd对应的数组元素位置并存储
	if (channelMap->list[fd] == NULL)
	{
		channelMap->list[fd] = channel;
		ret = evLoop->dispatcher->add(channel, evLoop);
	}

	return ret;
}

int eventLoopRemove(struct EventLoop* evLoop, struct Channel* channel)
{
	struct ChannelMap* channelMap = evLoop->channelMap;
	int fd = channel->fd;
	if (fd >= channelMap->size || channelMap->list[fd] == NULL)
	{
		return -1;
	}

	int ret = evLoop->dispatcher->remove(channel, evLoop);
	return ret;
}

int eventLoopModify(struct EventLoop* evLoop, struct Channel* channel)
{
	struct ChannelMap* channelMap = evLoop->channelMap;
	int fd = channel->fd;
	if (fd >= channelMap->size || channelMap->list[fd] == NULL)
	{
		return -1;
	}

	int ret = evLoop->dispatcher->modify(channel, evLoop);
	return ret;
}

int destoryChannel(struct EventLoop* evLoop, struct Channel* channel)
{
	int fd = channel->fd;

	// 删除channelMap和fd的对应关系
	evLoop->channelMap->list[fd] = NULL;

	// 关闭文件描述符
	close(fd);
	
    // 释放channel
	free(channel);

	return 0;
}



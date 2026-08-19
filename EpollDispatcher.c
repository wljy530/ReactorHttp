#include "Dispatcher.h"
#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>

#define Max 520  // epoll实例中events数组存放元素的最大个数
struct EpollData
{
	int epfd;
	struct epoll_event* events;
};

static void* epollInit();

// epollAdd，epollRemove，epollModify核心逻辑
static int epollCtl(struct Channel* channel, struct EventLoop* evLoop, int op);
static int epollAdd(struct Channel* channel, struct EventLoop* evLoop);
static int epollRemove(struct Channel* channel, struct EventLoop* evLoop);

static int epollModify(struct Channel* channel, struct EventLoop* evLoop);
static int epollDispatch(struct EventLoop* evLoop, int timeout);  // timeout单位: s
static int epollClear(struct EventLoop* evLoop);

struct Dispatcher EpollDispatcher = {
	epollInit,
	epollAdd,
	epollRemove,
	epollModify,
	epollDispatch,
	epollClear
};

static void* epollInit()
{
	struct EpollData* data = (struct EpollData*)malloc(sizeof(struct EpollData));

	data->epfd = epoll_create(1);
	if (data->epfd == -1)
	{
		perror("epoll_create");
		exit(0);
	}

	data->events = (struct epoll_event*)calloc(Max, sizeof(struct epoll_event));  // 申请内存的同时初始化为0

	return data;
}

int epollCtl(struct Channel* channel, struct EventLoop* evLoop, int op)
{
	struct EpollData* data = (struct EpollData*)evLoop->dispatcherData;

	struct epoll_event ev;
	ev.data.fd = channel->fd;

	/*
	   因为channel里的events的IO检测标准是根据自定义枚举类型fdEvent按位比较得到，
	   而epoll_ctl需要传入的参数的IO检测标准是Linux的标准，也就是EPOLLIN/EPOLLOUT，
	   所以这里不能直接将channel->events赋值给epoll_ctl的第四个参数
	*/
	unsigned short events = 0;
	if (channel->events & ReadEvent)
	{
		events |= EPOLLIN;
	}
	if (channel->events & WriteEvent)
	{
		events |= EPOLLOUT;
	}
	ev.events = events;

	int ret = epoll_ctl(data->epfd, op, channel->fd, &ev);

	return ret;
}

static int epollAdd(struct Channel* channel, struct EventLoop* evLoop)
{
	int ret = epollCtl(channel, evLoop, EPOLL_CTL_ADD);
	if (ret == -1)
	{
		perror("epoll_ctl add");
		exit(0);
	}
	return ret;
}

static int epollRemove(struct Channel* channel, struct EventLoop* evLoop)
{
	int ret = epollCtl(channel, evLoop, EPOLL_CTL_DEL);
	if (ret == -1)
	{
		perror("epoll_ctl delete");
		exit(0);
	}
	return ret;
}

static int epollModify(struct Channel* channel, struct EventLoop* evLoop)
{
	int ret = epollCtl(channel, evLoop, EPOLL_CTL_MOD);
	if (ret == -1)
	{
		perror("epoll_ctl modify");
		exit(0);
	}
	return ret;
}

static int epollDispatch(struct EventLoop* evLoop, int timeout)  // timeout单位: s
{
	struct EpollData* data = (struct EpollData*)evLoop->dispatcherData;

	int count = epoll_wait(data->epfd, data->events, Max, timeout * 1000);  // epoll_wait的第四个参数单位是ms
	if (count == -1)
	{
		perror("epoll_wait");
		exit(0);
	}

	for (int i = 0; i < count; ++i)
	{
		int fd = data->events->data.fd;
		int events = data->events->events;

		if (events & EPOLLERR || events & EPOLLHUP)  // 事件出现异常
		{
			// 对方断开连接，删除fd
			struct Channel* channel = evLoop->channelMap->list[fd];
			epollRemove(channel, evLoop);
			continue;
		}

		if (events & EPOLLIN)   // 读事件
		{
			eventActivate(evLoop, fd, ReadEvent);
		}
		if (events & EPOLLOUT)  // 写事件 
		{
			eventActivate(evLoop, fd, WriteEvent);
		}
	}

	return 0;
}

static int epollClear(struct EventLoop* evLoop)
{
	struct EpollData* data = (struct EpollData*)evLoop->dispatcherData;
	free(data->events);
	close(data->epfd);
	free(data);

	return 0;
}



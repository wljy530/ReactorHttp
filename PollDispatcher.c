#include "Dispatcher.h"
#include <sys/poll.h>
#include <stdio.h>
#include <stdlib.h>

#define Max 1024  // poll实例中fds数组存放元素的最大个数
struct PollData
{
	int maxfd;
	struct pollfd fds[Max];
};

static void* pollInit();
static int pollAdd(struct Channel* channel, struct EventLoop* evLoop);
static int pollRemove(struct Channel* channel, struct EventLoop* evLoop);
static int pollModify(struct Channel* channel, struct EventLoop* evLoop);
static int pollDispatch(struct EventLoop* evLoop, int timeout);  // timeout单位: s
static int pollClear(struct EventLoop* evLoop);

struct Dispatcher PollDispatcher = {
	pollInit,
	pollAdd,
	pollRemove,
	pollModify,
	pollDispatch,
	pollClear
};

static void* pollInit()
{
	struct PollData* data = (struct PollData*)malloc(sizeof(struct PollData));

	data->maxfd = 0;
	for (int i = 0; i < Max; ++i)
	{
		data->fds[i].fd = -1;
		data->fds[i].events = 0;
		data->fds[i].revents = 0;
	}

	return data;
}

static int pollAdd(struct Channel* channel, struct EventLoop* evLoop)
{
	struct PollData* data = (struct pollData*)evLoop->dispatcherData;
	
	unsigned short events = 0;
	if (channel->events & ReadEvent)
	{
		events |= POLLIN;
	}
	if (channel->events & WriteEvent)
	{
		events |= POLLOUT;
	}

	for (int i = 0; i < Max; ++i)
	{
		if (data->fds[i].fd == -1)
		{
			data->fds[i].fd = channel->fd;
			data->fds[i].events = events;
			data->maxfd = data->maxfd > i ? data->maxfd : i;
			
			return 0;
		}
	}
	
	return -1;
}

static int pollRemove(struct Channel* channel, struct EventLoop* evLoop)
{
	struct PollData* data = (struct pollData*)evLoop->dispatcherData;

	for (int i = 0; i <= data->maxfd; ++i)
	{
		if (data->fds[i].fd == channel->fd)
		{
			if (i != data->maxfd)  // 当前元素不是尾部元素
			{
				// 将最后一个有效元素覆盖到当前空位
				data->fds[i] = data->fds[data->maxfd];

				// 清空原来末尾位置
				data->fds[data->maxfd].fd = -1;
				data->fds[data->maxfd].events = 0;
				data->fds[data->maxfd].revents = 0;
			}
			else  // 当前元素就是尾部元素
			{
				data->fds[i].fd = -1;
				data->fds[i].events = 0;
				data->fds[i].revents = 0;
			}

			data->maxfd--;

			// 通过 channel 释放对应的 TcpConnection 资源
			channel->destroyCallback(channel->arg);  // 这里的arg是channelInit指定的conn(struct TcpConnection*)

			return 0;
		}
	}

	return -1;
}

static int pollModify(struct Channel* channel, struct EventLoop* evLoop)
{
	struct PollData* data = (struct PollData*)evLoop->dispatcherData;

	unsigned short events = 0;
	if (channel->events & ReadEvent)
	{
		events |= POLLIN;
	}
	if (channel->events & WriteEvent)
	{
		events |= POLLOUT;
	}

	for (int i = 0; i <= data->maxfd; ++i)
	{
		if (data->fds[i].fd == channel->fd)
		{
			data->fds[i].events = events;
			return 0;
		}
	}

	return -1;
}

static int pollDispatch(struct EventLoop* evLoop, int timeout)  // timeout单位: s
{
	struct PollData* data = (struct PollData*)evLoop->dispatcherData;

	int count = poll(data->fds, data->maxfd + 1, timeout * 1000);
	if (count == -1)
	{
		perror("poll");
		exit(0);
	}

	for (int i = 0; i <= data->maxfd; ++i)
	{
		int fd = data->fds[i].fd;
		if (fd == -1)
		{
			continue;
		}

		int revents = data->fds[i].revents;
		if (revents & POLLERR || revents & POLLHUP)  // 事件出现异常
		{
			// 对方断开连接，删除fd
			struct Channel* channel = evLoop->channelMap->list[fd];
			pollRemove(channel, evLoop);
			continue;
		}

		if (revents & POLLIN)   // 读事件
		{
			eventActivate(evLoop, fd, ReadEvent);
		}
		if (revents & POLLOUT)  // 写事件 
		{
			eventActivate(evLoop, fd, WriteEvent);
		}
	}

	return 0;
}

static int pollClear(struct EventLoop* evLoop)
{
	struct PollData* data = (struct PollData*)evLoop->dispatcherData;
	free(data);

	return 0;
}



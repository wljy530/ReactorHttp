#include "Channel.h"
#include <stdlib.h>

struct Channel* channelInit(int fd, int events, handleFunc readFunc, handleFunc writeFunc, 
	handleFunc destroyCallback, void* arg)
{
	struct Channel* channel = (struct Channel*)malloc(sizeof(struct Channel));
	channel->fd = fd;
	channel->events = events;
	channel->readCallback = readFunc;
	channel->writeCallback = writeFunc;
	channel->arg = arg;
	channel->destroyCallback = destroyCallback;

	return channel;
}

void writeEventEnable(struct Channel* channel, bool flag)
{
	if (flag)  // 增加写属性
	{
		channel->events = channel->events | WriteEvent;
	}
	else  // 删除写属性
	{
		channel->events = channel->events & ~WriteEvent;
	}
}

bool isWriteEventEnable(struct Channel* channel)
{
	return (channel->events & WriteEvent) == WriteEvent;
}

#pragma once
#include "Channel.h"
#include "EventLoop.h"

struct Dispatcher
{
	// 事件初始化 -- 初始化 epoll，poll 或者 select 需要的数据块
	void* (*init)();  // 返回值就是 epoll，poll 或者 select 需要的数据块

	// 事件添加
	int (*add)(struct Channel* channel, struct EventLoop* evLoop);

	// 事件删除
	int (*remove)(struct Channel* channel, struct EventLoop* evLoop);

	// 事件修改
	int (*modify)(struct Channel* channel, struct EventLoop* evLoop);

	// 事件检测
	int (*dispatch)(struct EventLoop* evLoop, int timeout);  // timeout单位: s

	// 清除数据(关闭fd或者释放内存)
	int (*clear)(struct EventLoop* evLoop);
};
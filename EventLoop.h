#pragma once
#include "Dispatcher.h"
#include "ChannelMap.h"
#include <pthread.h>

// 声明SelectDispatcher.c，PollDispatcher.c和EpollDispatcher.c中的全局变量
extern struct Dispatcher SelectDispatcher;
extern struct Dispatcher PollDispatcher;
extern struct Dispatcher EpollDispatcher;  

// 处理该节点中的channel的方式
enum ElemType
{
	ADD,      // 添加
	DELETE,   // 删除
	MODIFY    // 修改
};

// 定义任务队列的节点
struct ChannelElement
{
	int type;  // 如果处理该节点中的channel
	struct Channel* channel;
	struct ChannelElement* next;
};

struct EventLoop
{
	bool isQuit;
	struct Dispatcher* dispatcher;
	void* dispatcherData;
	
	// 任务队列
	struct ChannelElement* head;
	struct ChannelElement* tail;

	// map (处理文件描述符和channel结构体之间的关系)
	struct ChannelMap* channelMap;

	// 线程id，name，mutex
	pthread_t threadID;
	char pthreadName[32];
	pthread_mutex_t mutex;

	// 存储本地通信的fd，通过socketPair初始化
	int socketPair[2];
};

// 新任务只是加到用户态内存里的ChannelElement链表，这个队列对Linux内核完全不可见；内核只关心注册上去的fd有没有IO事件。
// 所以如果客户端没有发送数据，而子线程又在多路复用那块阻塞中，就会导致用户态新加入的任务永远无法解决。
// 因此将socketPair[1]加入任务队列，然后再通过taskWakeup发送数据给socketPair[0]，唤醒被多路复用阻塞中的子线程
void taskWakeup(struct EventLoop* evLoop);
int readLocalMessage(void* arg);

// 初始化
struct EventLoop* eventLoopInit();  // 只有一个主线程，直接起固定名字
struct EventLoop* eventLoopInitEx(const char* threadName);  // 方便子线程起名字

// 启动反应堆模型
int eventLoopRun(struct EventLoop* evLoop);

// 处理被激活的文件描述符的事件
int eventActivate(struct EventLoop* evLoop, int fd, int event);

// 添加任务到任务队列
int eventLoopAddTask(struct EventLoop* evLoop, struct Channel* channel, int type);

// 处理任务队列中的任务
int eventLoopProcessTask(struct EventLoop* evLoop);

// 建立节点与ChannelMap的联系，调用Dispatcher中的函数指针
int eventLoopAdd(struct EventLoop* evLoop, struct Channel* channel);
int eventLoopRemove(struct EventLoop* evLoop, struct Channel* channel);
int eventLoopModify(struct EventLoop* evLoop, struct Channel* channel);

// 释放channel
int destoryChannel(struct EventLoop* evLoop, struct Channel* channel);

#pragma once
#include <stdbool.h>

// 定义函数指针
typedef int (*handleFunc)(void* arg);

// 定义文件描述符的读写事件
enum fdEvent
{
	TimeOut = 0x01,     // 二进制 001
	ReadEvent = 0x02,   // 二进制 010
	WriteEvent = 0x04   // 二进制 100
};

struct Channel
{
	int fd;                      // 文件描述符
	int events;                  // 事件
	handleFunc readCallback;     // 读回调函数
	handleFunc writeCallback;    // 写回调函数 
	void* arg;                   // 回调函数的参数
	handleFunc destroyCallback;  // 通过此函数指针指向TcpConnection里的销毁函数
};

// 初始化一个Channel
struct Channel* channelInit(int fd, int events, handleFunc readFunc, handleFunc writeFunc,
	handleFunc destroyCallback, void* arg);

// 修改fd的写事件(检测 or 不检测)
void writeEventEnable(struct Channel* channel, bool flag);

// 判断是否需要检测文件描述符的写事件
bool isWriteEventEnable(struct Channel* channel);
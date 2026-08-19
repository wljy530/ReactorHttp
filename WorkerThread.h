#pragma once
#include <pthread.h>
#include "EventLoop.h"

// 定义子线程对应的结构体
struct WorkerThread
{
	pthread_t threadID;        // 线程ID
	char name[24];             // 线程名字
	pthread_mutex_t mutex;     // 互斥锁
	pthread_cond_t cond;	   // 条件变量
	struct EventLoop* evLoop;  // 反应堆模型
};

// 初始化(参数index主要用于起线程名字)
int workerThreadInit(struct WorkerThread* thread, int index);

// 子线程的回调函数
void* subThreadRunning(void* arg);
// 启动线程
void workerThreadRun(struct WorkerThread* thread);

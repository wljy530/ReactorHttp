#pragma once
#include "EventLoop.h"
#include "WorkerThread.h"

// 定义线程池结构体
struct ThreadPool
{
	bool isStart;   // 标记线程池是否启动
	int ThreadNum;  // 子线程总数量
	int index;      // 子线程编号
	struct WorkerThread* workerThreads;  // 存储子线程的数组

	// 主线程的反应堆模型
	struct EventLoop* mainLoop;
};

// 初始化线程池(传入子线程个数)
struct ThreadPool* threadPoolInit(struct EventLoop* mainLoop, int count);

// 启动线程池
void threadPoolRun(struct ThreadPool* pool);

// 取出线程池中的某个子线程的反应堆实例
struct EventLoop* takeWorkerEventLoop(struct ThreadPool* pool);
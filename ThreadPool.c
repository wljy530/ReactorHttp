#include "ThreadPool.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

struct ThreadPool* threadPoolInit(struct EventLoop* mainLoop, int count)
{
	struct ThreadPool* pool = (struct ThreadPool*)malloc(sizeof(struct ThreadPool));
	pool->isStart = false;
	pool->ThreadNum = count;
	pool->index = 0;
	pool->mainLoop = mainLoop;
	pool->workerThreads = (struct WorkerThread*)malloc(count * sizeof(struct WorkerThread));

	return pool;
}

void threadPoolRun(struct ThreadPool* pool)
{
	assert(pool && !pool->isStart);  // 断言确保线程池存在并且未启动
	if (pool->mainLoop->threadID != pthread_self())  // 确保执行当前函数的线程是主线程
	{
		exit(0);
	}

	pool->isStart = true;
	if (pool->ThreadNum > 0)  
	{
		for (int i = 0; i < pool->ThreadNum; ++i)
		{
			workerThreadInit(&pool->workerThreads[i], i);  // 初始化子线程实例
			workerThreadRun(&pool->workerThreads[i]);      // 启动子线程
		}
	}
}

struct EventLoop* takeWorkerEventLoop(struct ThreadPool* pool)
{
	assert(pool->isStart);
	if (pool->mainLoop->threadID != pthread_self())  // 确保执行当前函数的线程是主线程
	{
		exit(0);
	}

	// 从线程池中找一个子线程，然后取出里面的反应堆实例
	// 当线程池中没有子线程时，就只好让主线程参与IO事件，因此先将指针初始化为主线程中的反应堆实例
	struct EventLoop* evLoop = pool->mainLoop;  
	if (pool->ThreadNum > 0)
	{
		evLoop = pool->workerThreads[pool->index].evLoop;
		pool->index = (pool->index + 1) % pool->ThreadNum;  // 这里对子线程编号进行处理，防止所有任务全交给一个线程
	}

	return evLoop;
}

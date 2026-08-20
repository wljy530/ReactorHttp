#include "WorkerThread.h"
#include <stdio.h>

int workerThreadInit(struct WorkerThread* thread, int index)
{
	thread->threadID = 0;
	sprintf(thread->name, "SubThread-%d", index);
	pthread_mutex_init(&thread->mutex, NULL);
	pthread_cond_init(&thread->cond, NULL);
	thread->evLoop = NULL;

	return 0;
}

void* subThreadRunning(void* arg)  
{
	struct WorkerThread* thread = (struct WorkerThread*)arg;

	pthread_mutex_lock(&thread->mutex);
	thread->evLoop = eventLoopInitEx(thread->name);  // 初始化反应堆模型
	pthread_mutex_unlock(&thread->mutex);
	pthread_cond_signal(&thread->cond);  // 唤醒阻塞中的主线程

	eventLoopRun(thread->evLoop);  // 启动反应堆模型
	return NULL;
}

void workerThreadRun(struct WorkerThread* thread)
{
	// 创建子线程
	pthread_create(&thread->threadID, NULL, subThreadRunning, thread);  // 向回调函数传参子线程对应的结构体

	// 由于创建子线程时传入的回调函数里有对反应堆模型初始化，这也需要一定的时间
	// 如果没有初始化结束，主线程就对子线程进行调用就是未定义的错误
	// 为了保证当前函数返回时，子线程与反应堆模型一定初始化成功，可以利用条件变量阻塞主线程，让当前函数不直接结束
	pthread_mutex_lock(&thread->mutex);
	while (thread->evLoop == NULL)  // while防止虚假唤醒
	{
		pthread_cond_wait(&thread->cond, &thread->mutex);
	}
	pthread_mutex_unlock(&thread->mutex);
}

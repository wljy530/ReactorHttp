#pragma once
#include "Channel.h"

struct ChannelMap
{
	int size;  // 记录list指针指向数组的元素个数
	struct Channel** list;  // struct Channel* list[]
};

// 初始化ChannelMap结构体
struct ChannelMap* channelMapInit(int size);

// 清空map数据
void ChannelMapClear(struct ChannelMap* map);

// 扩容(重新分配内存空间)
bool makeMapRoom(struct ChannelMap* map, int newSize, int unitSize);
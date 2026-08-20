#pragma once

struct Buffer
{
	char* data;     // 指向内存的指针
	int readPos;    // 读数据的位置(data + readPos)
	int writePos;   // 写数据的位置(data + writePos)
	int capacity;   // buffer内存块的总大小(总字节数)
};

// 初始化
struct Buffer* bufferInit(int size);

// 得到剩余的可读的内存容量
int bufferReadableSize(struct Buffer* buffer);

// 得到剩余的可写的内存容量
int bufferWriteableSize(struct Buffer* buffer);

// 扩容
void bufferExtendRoom(struct Buffer* buffer, int size);

// 写内存 
// 1. 将字符串data写入buffer
// 这里多一个参数size描述需要拷贝的字符串长度而不用strlen是为了防止字符串中带有'\0'这样的特殊字符
int bufferAppendData(struct Buffer* buffer, const char* data, int size);
int bufferAppendString(struct Buffer* buffer, const char* data);
// 2. 接收套接字数据
int bufferSocketRead(struct Buffer* buffer, int fd);

// 根据\r\n取出一行(作用于解析协议的时候)，找到其在数据块中的位置，返回该位置
char* bufferFindCRLF(struct Buffer* buffer);

// 销毁内存
void bufferDestroy(struct Buffer* buffer);
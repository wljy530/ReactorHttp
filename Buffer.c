#define _GNU_SOURCE 
#include "Buffer.h"
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

struct Buffer* bufferInit(int size)
{
	struct Buffer* buffer = (struct Buffer*)malloc(sizeof(struct Buffer));
	if (buffer != NULL)
	{
		buffer->data = (char*)malloc(size);
		memset(buffer->data, 0, size);
		buffer->readPos = 0;
		buffer->writePos = 0;
		buffer->capacity = size;
	}

	return buffer;
}

int bufferReadableSize(struct Buffer* buffer)
{
	return buffer->writePos - buffer->readPos;
}

int bufferWriteableSize(struct Buffer* buffer)
{
	return buffer->capacity - buffer->writePos;
}

void bufferExtendRoom(struct Buffer* buffer, int size)
{
	// 1. 内存够用
	if (bufferWriteableSize(buffer) >= size)
	{
		return;
	}
	// 2. 内存需要合并才够用
	// 已读的内存 + 剩余的可写的内存 >= size
	else if (buffer->readPos + bufferWriteableSize(buffer) >= size)
	{
		// 得到未读的内存大小
		int readableSize = bufferReadableSize(buffer);
		// 移动内存，将未读内存移到已读内存前面
		memcpy(buffer->data, buffer->data + buffer->readPos, readableSize);
		// 更新位置
		buffer->readPos = 0;
		buffer->writePos = readableSize;
	}
	// 3. 内存不够用，需要扩容
	else
	{
		void* temp = realloc(buffer->data, buffer->capacity + size);
		if (temp == NULL)
		{
			return;
		}

		// 更新数据
		memset(temp + buffer->capacity, 0, size);  // 这里只将扩容后的内存初始化为0
		buffer->data = temp;
		buffer->capacity += size;
	}
}

int bufferAppendData(struct Buffer* buffer, const char* data, int size)
{
	if (buffer == NULL || data == NULL || size <= 0)
	{
		return -1;
	}

	// 需要扩容
	if (bufferWriteableSize(buffer) < size)
	{
		bufferExtendRoom(buffer, size);
	}

	// 数据拷贝
	memcpy(buffer->data + buffer->writePos, data, size);
	buffer->writePos += size;

	return 0;
}

int bufferAppendString(struct Buffer* buffer, const char* data)
{
	int size = strlen(data);
	int ret = bufferAppendData(buffer, data, size);

	return ret;
}

int bufferSocketRead(struct Buffer* buffer, int fd)
{
	// read/recv/readv
	struct iovec vec[2];

	// 初始化数组元素
	int writeableSize = bufferWriteableSize(buffer);
	vec[0].iov_base = buffer->data + buffer->writePos;
	vec[0].iov_len = writeableSize;
	char* tmpbuf = (char*)malloc(40960);
	vec[1].iov_base = tmpbuf;
	vec[1].iov_len = 40960;

	// 读取数据
	int result = readv(fd, vec, 2);
	if (result == -1)
	{
		perror("readv");
		return -1;
	}
	else if (result <= writeableSize)  // 数据完全写入buffer，无需扩容
	{
		buffer->writePos += result;
	}
	else if (result > writeableSize)
	{
		buffer->writePos = buffer->capacity;  // buffer已经写满
		bufferAppendData(buffer, tmpbuf, result - writeableSize);  // 该函数内置了扩容函数，可以完美应对三种情况
	}

	free(tmpbuf);
	return result;
}

char* bufferFindCRLF(struct Buffer* buffer)
{
	// strstr --> 大字符串中匹配子字符串(遇到\0结束)
	// memmem --> 大数据块中匹配子数据块(根据指定数据块大小查找)
	char* ptr = memmem(buffer->data + buffer->readPos, bufferReadableSize(buffer), "\r\n", 2);

	return ptr;
}

int bufferSendData(struct Buffer* buffer, int socket)
{
	// 判断buffer有无数据
	int readable = bufferReadableSize(buffer);  // 待发送给客户端的数据块
	if (readable > 0)
	{
		// send函数的第四个参数需要传入宏值MSG_NOSIGNAL，这是由于如果客户端将未下载好的文件关闭或者未加载好的页面返回，
		// 相当于关闭了TCP的读端，但是服务器继续向读端写数据，就会触发SIGPIPE(管道破裂)，让服务器终止，这对服务器是致命的
		// 因此这里传入宏值忽略该信号
		int count = send(socket, buffer->data + buffer->readPos, readable, MSG_NOSIGNAL);
		if (count > 0)
		{
			buffer->readPos += count;
			//usleep(1);  // 这非常重要，可以给客户端解析数据留有一定的缓冲时间(但是发送数据变得很慢)
		}
		return count;
	}

	return 0;
}

void bufferDestroy(struct Buffer* buffer)
{
	if (buffer != NULL)
	{
		if (buffer->data != NULL)
		{
			free(buffer->data);
			buffer->data = NULL;
		}
		free(buffer);
		buffer = NULL;
	}
}

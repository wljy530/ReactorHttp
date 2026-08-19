#include "Buffer.h"
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <stdio.h>

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
	vec[0].iov_base = buffer->data + buffer->readPos;
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

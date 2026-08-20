#define _GNU_SOURCE 
#include "HttpRequest.h"
#include "Buffer.h"
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <assert.h>

#define HeaderSize 12  // 初始化时请求头的键值对数量
struct HttpRequest* httpRequestInit()
{
	struct HttpRequest* request = (struct HttpRequest*)malloc(sizeof(struct HttpRequest));
	httpRequestReset(request);
	request->reqHeaders = (struct RequestHeader*)malloc(HeaderSize * sizeof(struct RequestHeader));

	return request;
}

void httpRequestReset(struct HttpRequest* request)
{
	request->method = NULL;
	request->url = NULL;
	request->version = NULL;
	request->reqHeaderNum = 0;
	request->curState = ParseReqLine;
}

void httpRequestResetEx(struct HttpRequest* request)
{
	free(request->method);
	free(request->url);
	free(request->version);

	if (request->reqHeaders != NULL)
	{
		for (int i = 0; i < request->reqHeaderNum; ++i)
		{
			free(request->reqHeaders[i].key);
			free(request->reqHeaders[i].value);
		}
		free(request->reqHeaders);
	}

	httpRequestReset(request);
}

void httpRequestDestroy(struct HttpRequest* request)
{
	if (request != NULL)
	{
		httpRequestResetEx(request);
		free(request);
	}
}

enum HttpRequestState getHttpRequestState(struct HttpRequest* request)
{
	return request->curState;
}

void httpRequestAddHeader(struct HttpRequest* request, const char* key, const char* value)
{
	int curIndex = request->reqHeaderNum;
	request->reqHeaders[curIndex].key = key;
	request->reqHeaders[curIndex].value = value;
	request->reqHeaderNum++;
}

char* httpRequestGetHeader(struct HttpRequest* request, const char* key)
{
	if (request != NULL)
	{
		for (int i = 0; i < request->reqHeaderNum; ++i)
		{
			// 比较字符串是否相等(忽视字符大小写)
			if (strncasecmp(request->reqHeaders[i].key, key, strlen(key)) == 0)
			{
				return request->reqHeaders[i].value;
			}
		}
	}

	return NULL;
}

bool parseHttpRequestLine(struct HttpRequest* request, struct Buffer* readBuf)
{
	// 读出请求行，保存字符串结束地址
	char* end = bufferFindCRLF(readBuf);
	// 保存字符串起始地址
	char* start = readBuf->data + readBuf->readPos;
	// 请求行总长度
	int lineSize = end - start;

	if (lineSize > 0)
	{
		// get /xxx/xx.txt http/1.1
		// 请求方式
		char* space = memmem(start, lineSize, " ", 1);  // 指向请求方式后的空格
		assert(space != NULL);
		int methodSize = space - start;
		request->method = (char*)malloc(methodSize + 1);  // 多加的1是存储字符串结束符'\0'
		strncpy(request->method, start, methodSize);  // 此函数不会主动加'\0'
		request->method[methodSize] = '\0';

		// 请求的静态资源
		start = space + 1;
		space = memmem(start, end - start, " ", 1);  // 指向静态资源后的空格
		assert(space != NULL);
		int urlSize = space - start;
		request->url = (char*)malloc(urlSize + 1);  
		strncpy(request->url, start, urlSize);  
		request->url[urlSize] = '\0';

		// http版本
		start = space + 1;
		request->version = (char*)malloc(end - start + 1);
		strncpy(request->version, start, end - start);
		request->version[end - start] = '\0';

		// 为解析请求头做准备
		readBuf->readPos += lineSize + 2;     // 还需要加上解析行尾的\r\n
		request->curState = ParseReqHeaders;  // 修改状态为开始解析请求头
		
		return true;
	}

	return false;
}

bool parseHttpRequestHeader(struct HttpRequest* request, struct Buffer* readBuf)
{
	// 请求头中的一行的结束地址
	char* end = bufferFindCRLF(readBuf);  
	if (end != NULL)
	{
		// 请求头中的一行的开始地址
		char* start = readBuf->data + readBuf->readPos;
		// 当前行的长度
		int lineSize = end - start;

		// 基于': '搜索字符串(标准写法：key: value，:后面有空格)
		char* middle = memmem(start, lineSize, ": ", 2);
		if (middle != NULL)
		{
			char* key = malloc(middle - start + 1);  // 多加的1是存储字符串结束符'\0'
			strncpy(key, start, middle - start);
			key[middle - start] = '\0';
			
			start = middle + 2;
			char* value = malloc(end - start + 1);  
			strncpy(value, start, end - start);
			value[end - start] = '\0';

			httpRequestAddHeader(request, key, value);  // 添加请求头

			// 移动读数据的位置
			readBuf->readPos += lineSize + 2;  // 还需要加上解析行尾的\r\n
		}
		else
		{
			// 请求头已经被解析完了，跳过空行
			readBuf->readPos += 2;

			// 修改解析状态
			if (strcasecmp(request->method, "get") == 0)  // get请求方式后没有数据块，到此解析结束
			{
				request->curState = ParseReqDone;  // 将状态修改为解析结束
			}
			else if (strcasecmp(request->method, "post") == 0)  // post请求方式后还有数据块
			{
				request->curState = ParseReqBody;  // 将状态修改为解析数据块
			}
		}

		return true;
	}

	return false;
}

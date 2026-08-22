#pragma once
#include "TcpConnection.h"
#include "Buffer.h"

// 定义状态码
enum HttpStatusCode
{
	UnKnow,                 // 未知
	OK = 200,               // 服务器对客户端的请求处理成功
	MovePermanently = 301,  // 永久重定向     
	MoveTemporarily = 302,  // 临时重定向
	BadRequest = 400,		// 客户端发起的是错误请求
	NotFound = 404			// 客户端想服务器请求的静态资源不存在   
};

// 定义响应头中键值对的结构体
struct ResponseHeader
{
	char key[32];
	char value[128];
};

// 定义一个函数指针，用来组织要回复给客户端的数据块
typedef void (*responseBody)(const char* fileName, struct Buffer* sendBuf, int socket);

// 定义结构体
struct HttpResponse
{
	char fileName[128];

	// 状态行: 状态码，状态描述
	enum HttpStatusCode statusCode;  // 状态码
	char statusMsg[128];  // 状态描述

	// 响应头 - 键值对
	struct ResponseHeader* headers;
	int headerNum;  // 键值对数量

	// 组织要回复给客户端的数据块
	responseBody sendDataFunc;
};

// 初始化
struct HttpResponse* httpResponseInit();

// 销毁
void httpResponseDestroy(struct HttpResponse* response);

// 添加响应头
void httpResponseAddHeader(struct HttpResponse* response, const char* key, const char* value);

// 组织http响应数据
void httpResponsePrepareMsg(struct HttpResponse* response, struct Buffer* sendBuf, int socket);
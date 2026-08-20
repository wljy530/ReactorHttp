#pragma once
#include <stdbool.h>

// 请求头键值对
struct RequestHeader
{
	char* key;
	char* value;
};

// 当前的解析状态
enum HttpRequestState
{
	ParseReqLine,     // 当前正在解析请求行  
	ParseReqHeaders,  // 当前正在解析请求头
	ParseReqBody,     // 当前正在解析请求的数据块
	ParseReqDone      // 完成http协议的解析
};
// 定义http请求结构体
struct HttpRequest
{
	// 请求行
	char* method;   // 请求方式
	char* url;      // 请求资源
	char* version;  // 协议版本
	// 请求头
	struct RequestHeader* reqHeaders;
	// 有效键值对数量
	int reqHeaderNum;
	// 当前解析状态
	enum HttpRequestState curState;
};

// 初始化
struct HttpRequest* httpRequestInit();

// 重置HttpRequest实例
void httpRequestReset(struct HttpRequest* request);
void httpRequestResetEx(struct HttpRequest* request);  // 额外释放请求行和请求头内存

// 释放HttpRequest实例
void httpRequestDestroy(struct HttpRequest* request);  // 释放请求行、请求头和HttpRequest实例内存

// 获取当前解析状态
enum HttpRequestState getHttpRequestState(struct HttpRequest* request);

// 添加请求头(注意参数传入的key和value并没有在函数内部申请内存，因此需要保证参数是全局变量或者堆内存)
void httpRequestAddHeader(struct HttpRequest* request, const char* key, const char* value);

// 根据key值得到对应请求头的value值
char* httpRequestGetHeader(struct HttpRequest* request, const char* key);

// 解析请求行
bool parseHttpRequestLine(struct HttpRequest* request, struct Buffer* readBuf);

// 解析请求头(该函数处理请求头中的一行)
bool parseHttpRequestHeader(struct HttpRequest* request, struct Buffer* readBuf);
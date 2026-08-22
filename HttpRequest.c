#define _GNU_SOURCE 
#include "HttpRequest.h"
#include "Buffer.h"
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <sys/stat.h>
#include "HttpResponse.h"
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>

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

bool parseHttpRequest(struct HttpRequest* request, struct Buffer* readBuf, 
	struct HttpResponse* response, struct Buffer* sendBuf, int socket)
{
	bool flag = true;
	while (request->curState != ParseReqDone)
	{
		switch (request->curState)
		{
		case ParseReqLine:
			flag = parseHttpRequestLine(request, readBuf);
			break;
		case ParseReqHeaders:
			flag = parseHttpRequestHeader(request, readBuf);
			break;
		case ParseReqBody:  // 对于post请求暂时不做处理
			break;
		default:
			break;
		}

		if (!flag)
		{
			return flag;
		}

		// 判断是否解析完毕了，如果完毕了，需要准备回复的数据
		if (request->curState == ParseReqDone)  
		{
			// 1. 根据解析出的原始数据，对客户端的请求做出处理
			processHttpRequest(request, response);

			// 2. 组织响应数据并发送给客户端
			httpResponsePrepareMsg(response, sendBuf, socket);
		}
	}
	request->curState = ParseReqLine;  // 状态还原，保证还能继续处理第二条及以后的请求

	return flag;
}

int hexToDec(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	return 0;
}

void decodeMsg(char* to, char* from)
{
	for (; *from != '\0'; ++to, ++from)
	{
		// isxdigit -> 判断字符是不是16进制格式，取值在 0-f
		// Linux%E5%9B%BE%E8%A7%A3.jpg  (%XX 是 URL 编码标准格式：固定占用 3 个字符)
		if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
		{
			// 将16进制的数 -> 十进制 将这个数值赋值给字符 int -> char
			// B2 == 178
			// 将三个字符，变成一个字符，这个字符就是原始数据
			*to = hexToDec(from[1]) * 16 + hexToDec(from[2]);

			// 跳过 from[1] 和 from[2] 因此在当前循环已经处理过了
			from += 2;
		}
		else
		{
			// 字符拷贝，赋值
			*to = *from;
		}
	}

	*to = '\0';
}

const char* getFileType(const char* name)
{
	// a.jpg a.mp4 a.html
	// 自右向左找 '.' 字符，如不存在返回NULL
	const char* dot = strrchr(name, '.');
	// 无后缀默认纯文本
	if (dot == NULL)
		return "text/plain; charset=utf-8";

	if (strcasecmp(dot, ".html") == 0 || strcasecmp(dot, ".htm") == 0)
		return "text/html; charset=utf-8";
	else if (strcasecmp(dot, ".css") == 0)
		return "text/css; charset=utf-8";
	else if (strcasecmp(dot, ".js") == 0)
		return "application/javascript; charset=utf-8";
	else if (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0)
		return "image/jpeg";
	else if (strcasecmp(dot, ".png") == 0)
		return "image/png";
	else if (strcasecmp(dot, ".gif") == 0)
		return "image/gif";
	else if (strcasecmp(dot, ".ico") == 0)
		return "image/x-icon";
	else if (strcasecmp(dot, ".svg") == 0)
		return "image/svg+xml";
	else if (strcasecmp(dot, ".mp4") == 0)
		return "video/mp4";
	else if (strcasecmp(dot, ".mov") == 0)
		return "video/quicktime";
	else if (strcasecmp(dot, ".avi") == 0)
		return "video/x-msvideo";
	else if (strcasecmp(dot, ".mpeg") == 0 || strcasecmp(dot, ".mpe") == 0)
		return "video/mpeg";
	else if (strcasecmp(dot, ".midi") == 0 || strcasecmp(dot, ".mid") == 0)
		return "audio/midi";
	else if (strcasecmp(dot, ".ogg") == 0)
		return "audio/ogg";
	else if (strcasecmp(dot, ".wav") == 0)
		return "audio/wav";
	else if (strcasecmp(dot, ".au") == 0)
		return "audio/basic";
	else if (strcasecmp(dot, ".vrml") == 0 || strcasecmp(dot, ".wrl") == 0)
		return "model/vrml";
	else if (strcasecmp(dot, ".pac") == 0)
		return "application/x-ns-proxy-autoconfig";
	else if (strcasecmp(dot, ".mp3") == 0)
		return "audio/mpeg";
	else if (strcasecmp(dot, ".txt") == 0)
		return "text/plain; charset=utf-8";
	else if (strcasecmp(dot, ".json") == 0)
		return "application/json; charset=utf-8";
	else if (strcasecmp(dot, ".pdf") == 0)
		return "application/pdf";

	// 未知后缀
	return "text/plain; charset=utf-8";
}

/*
<html>
	<head>
		<title>test</title>
	</head>
	<body>
		<table>
			<tr>  行
				<td></td>  列
				<td></td>
			</tr>
			<tr>  行
				<td></td>  列
				<td></td>
			</tr>
		</table>
	</body>
</html>
*/
void sendDir(const char* dirName, struct Buffer* sendBuf, int cfd)
{
	// 需要发送的html标签语言
	char buf[4096] = { 0 };
	sprintf(buf, "<html><head><title>%s</title></head><body><table>", dirName);

	// 对目录遍历获取目录下文件名
	struct dirent** namelist;
	int num = scandir(dirName, &namelist, NULL, alphasort);
	for (int i = 0; i < num; i++)
	{
		char* name = namelist[i]->d_name;  // 目录dirName下的文件名

		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		{
			free(namelist[i]);
			continue;
		}

		// 拼接完整目录路径
		char subPath[1024] = { 0 };
		sprintf(subPath, "%s/%s", dirName, name);

		struct stat st;
		stat(subPath, &st);
		if (S_ISDIR(st.st_mode))  // 如果是目录
		{
			// a标签跳转页面 <a href="目标地址">name</a>
			sprintf(buf + strlen(buf), "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>"
				, name, name, st.st_size);
		}
		else  // 其他文件
		{
			// 如果不是目录第一个%s后就不需要加/
			sprintf(buf + strlen(buf), "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>"
				, name, name, st.st_size);
		}

		bufferAppendString(sendBuf, buf);
		memset(buf, 0, sizeof(buf));

		free(namelist[i]);
	}

	sprintf(buf, "</table></body></html>");
	bufferAppendString(sendBuf, buf);

	free(namelist);
}

void sendFile(const char* fileName, struct Buffer* sendBuf, int cfd)
{
	int fd = open(fileName, O_RDONLY);
	assert(fd > 0);

#if 1
	while (1)
	{
		char buf[1024];
		int len = read(fd, buf, sizeof(buf));
		if (len > 0)
		{
			// bufferAppendString(sendBuf, buf); 这里不用这个函数，因为此函数内部的strlen需要字符串尾部有\0
			bufferAppendData(sendBuf, buf, len);
			usleep(10);  // 这非常重要，可以给客户端解析数据留有一定的缓冲时间
		}
		else if (len == 0)
		{
			break;
		}
		else
		{
			perror("read");
			close(fd);
		}
	}
#else
	int size = lseek(fd, 0, SEEK_END);  // 获取文件大小
	lseek(fd, 0, SEEK_SET);  // 将文件指针移回开头

	off_t offset = 0;
	while (offset < size)
	{
		int ret = sendfile(cfd, fd, &offset, size - offset);  // 这种发送数据的方式在内核，因此更加高效

		if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))  // tcp缓冲区满了，需要阻塞等待
		{
			usleep(1000); // 休眠1ms，避免疯狂刷屏
			continue;
		}

		printf("ret value: %d\n", ret);
		if (ret == -1)
		{
			perror("sendfile");
		}
	}

#endif
	close(fd);
}

bool processHttpRequest(struct HttpRequest* request, struct HttpResponse* response)
{
	if (strcasecmp(request->method, "get") != 0)
	{
		return -1;
	}
	// 将UTF-8编码格式中的中文乱码恢复正常
	decodeMsg(request->url, request->url);

	// 处理客户端请求的静态资源(目录或者文件)
	// 前提：在main函数已经将工作目录改到资源文件根目录了
	// 获取请求资源文件所在目录
	char* file = NULL;
	if (strcmp(request->url, "/") == 0)  // 判断客户端访问的是否正好就是网站虚拟根目录 /
	{
		file = "./";
	}
	else
	{
		file = request->url + 1;  // 去掉URL网站虚拟根目录/，只赋值/后面的字符串
	}

	// 获取文件属性
	struct stat st;
	int ret = stat(file, &st);
	if (ret == -1)
	{
		// 文件不存在 -- 回复404
		strcpy(response->fileName, "404.html");  // 文件名
		response->statusCode = NotFound;         // 状态码
		strcpy(response->statusMsg, "NotFound"); // 状态描述 
		// 响应头
		httpResponseAddHeader(response, "Content-type", getFileType(".html"));
		response->sendDataFunc = sendFile;

		return false;
	}
	
	// 判断文件类型
	if (S_ISDIR(st.st_mode))  // 判断是否是目录
	{
		// 把本地目录的内容发送给客户端
		strcpy(response->fileName, file);  // 文件名
		response->statusCode = OK;         // 状态码
		strcpy(response->statusMsg, "OK"); // 状态描述 
		// 响应头
		httpResponseAddHeader(response, "Content-type", getFileType(".html"));
		response->sendDataFunc = sendDir;
	}
	else
	{
		// 把文件的内容发送给客户端
		strcpy(response->fileName, file);  // 文件名
		response->statusCode = OK;         // 状态码
		strcpy(response->statusMsg, "OK"); // 状态描述 
		// 响应头
		char tmp[12] = { 0 };
		sprintf(tmp, "ld", st.st_size);  
		httpRequestAddHeader(request, "Content-length", tmp);  // 文件大小
		httpResponseAddHeader(response, "Content-type", getFileType(file));
		response->sendDataFunc = sendFile;
	}

	return true;
}

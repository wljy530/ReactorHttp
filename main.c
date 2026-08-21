#include <stdio.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		printf("You should input it in this format: ./a.out port path\n");
	}

	// 将端口字符串转换成整型
	unsigned short port = atoi(argv[1]);  

	// 切换服务器的工作路径
	chdir(argv[2]);

	return 0;
}
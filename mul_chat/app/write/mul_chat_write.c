#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#define mul_chat  "/dev/mul_chat" 

int main(int argc, char **argv)
{
	int fd;
	char msg[100];
	fd = open(mul_chat, O_RDWR);
	if(fd >= 0)
	{
		while(1)
		{
			printf("Please input the data:");
			scanf("%s", msg);
			if(strlen(msg) >= 100)
			{
				printf("data too long!\n");
				return -1;
			}
			write(fd, msg, strlen(msg));
			if(strcmp(msg, "quit") == 0)
			{
				close(fd);
				break;
			}
		}
	}
	else
	{
		printf("device open failed\n");
		return -1;
	}
	return 0;
}
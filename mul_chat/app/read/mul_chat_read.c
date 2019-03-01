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
	char msg[101];
	fd = open(mul_chat, O_RDWR);
	if(fd >= 0)
	{
		while(1)
		{
			memset(msg, 0, sizeof(msg));
			printf("the receive data is: ");
			read(fd, msg, 100);
			printf("%s\n", msg);
			if(strcmp(msg, "quit") == 0)
			{
				close(fd);
				break;
			}
		}
	}
	else
	{
		printf("device open failed, %d\n", fd);
		return -1;
	}
		
	return 0;
}
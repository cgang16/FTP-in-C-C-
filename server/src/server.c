#include "Usr.h"
#include "func.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <memory.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <pthread.h>

// global variable
char root_dir[256] = "/tmp"; // initial root dir
char server_ip[16] = "127.0.0.1";

// only available in this file
static int listen_port = 21;  // listen port
static int client_num = 0;	// client number
static int connfd[10];		  // command transfer fd
static pthread_t threads[10]; // threads
// commad type and str
typedef enum
{
	USER,
	PASS,
	RETR,
	STOR,
	QUIT,
	ABOR,
	SYST,
	TYPE,
	PORT,
	PASV,
	MKD,
	CWD,
	PWD,
	LIST,
	RMD,
	RNFR,
	RNTO,
	REST
} CMD_TYPE;
static char *CMD_STR[] = {"USER", "PASS", "RETR", "STOR", "QUIT", "ABOR", "SYST", "TYPE", "PORT", "PASV", "MKD", "CWD",
						  "PWD", "LIST", "RMD", "RNFR", "RNTO", "REST"};

// get command line argument
int getCommandLineArg(int argc, char **argv);

// find available thread
int findThread();

int resetThread(int fd);

// get command type
int getCmdtype(char request[]);

// thread function
void *processRequest(void *connfd);

int main(int argc, char **argv)
{

	srand(time(NULL));

	memset(connfd, -1, sizeof(connfd));

	// get command line argument
	if (getCommandLineArg(argc, argv) == 1)
	{
		return 1;
	}

	int listenfd;
	struct sockaddr_in addr;

	// create listen socket
	if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	// bind port
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(listen_port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// bind socket and port
	if (bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	// listen at the port, maximum waiting num:10
	if (listen(listenfd, 10) == -1)
	{
		printf("Error listen(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	char buffer[64] = "220 Anonymous FTP server ready.\r\n";

	// use loop to handle all the reqeust
	while (1)
	{

		// get available thread
		if (client_num == 10)
		{
			continue;
		}
		int index = findThread();
		if (index == -1)
		{
			continue;
		}
		// accept error
		if ((connfd[index] = accept(listenfd, NULL, NULL)) == -1)
		{
			printf("Error accept(): %s(%d)\n", strerror(errno), errno);
			continue;
		}
		// initial greeting
		if (send(connfd[index], buffer, strlen(buffer), 0) == -1)
		{
			close(connfd[index]);
			connfd[index] = -1;
			printf("Error initial response failed: %s(%d)!\n", strerror(errno), errno);
			continue;
		}
		// create a new thread
		if ((pthread_create(&threads[index], NULL, processRequest, (void *)&connfd[index])) != 0)
		{
			close(connfd[index]);
			connfd[index] = -1;
			printf("Error pthread_create(): %s(%d)\n", strerror(errno), errno);
			continue;
		}
		client_num++;
	}

	close(listenfd);
	return 0;
}

// get command line argument
int getCommandLineArg(int argc, char **argv)
{
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "-root") == 0)
		{
			if (i + 1 >= argc)
			{
				printf("Error: need root directory.\n");
				return 1;
			}
			strcpy(root_dir, argv[++i]);
			DIR *dp;
			if ((dp = opendir(root_dir)) == NULL)
			{
				printf("Error: Please input a legal root dir.\n");
				return 1;
			}
			closedir(dp);
		}
		else if (strcmp(argv[i], "-port") == 0)
		{
			if (i + 1 >= argc)
			{
				printf("Error: need port number.\n");
				return 1;
			}
			listen_port = atoi(argv[++i]);
			if (listen_port == 0)
			{
				printf("Error: Please input a legal port number.\n");
				return 1;
			}
		}
		else
		{
			printf("Error: Illegal command line arguments.\n");
			return 1;
		}
	}
	return 0;
}

// find available thread
int findThread()
{
	for (int i = 0; i < 10; ++i)
	{
		if (connfd[i] == -1)
		{
			return i;
		}
	}
	return -1;
}

int resetThread(int fd)
{
	for (int i = 0; i < 10; ++i)
	{
		if (connfd[i] == fd)
		{
			connfd[i] = -1;
			return i;
		}
	}
	return -1;
}

// get command type
int getCmdtype(char request[])
{
	for (int i = 0; i < 18; ++i)
	{
		if (strncmp(request, CMD_STR[i], strlen(CMD_STR[i])) == 0)
		{
			return i;
		}
	}
	return -1;
}

// thread function
void *processRequest(void *connfd)
{
	int cfd = *((int *)connfd);

	char *request = (char *)malloc(8192);
	memset(request, '\0', 8192);

	Usr *usr = (Usr *)malloc(sizeof(Usr));
	initializeUsr(usr);

	int cmd_type = -1;

	// initial greeting
	while (1)
	{
		int n = 0;
		// get request content
		if (getRequest(cfd, request) == -1 || strlen(request) == 0)
		{
			printf("Error: get request content failed.\n");
			break;
		}
		// get cmd type
		if ((cmd_type = getCmdtype(request)) == -1)
		{
			printf("Error: Command:%s\n", request);
			if (respWRONGREQ(cfd, usr) == -1)
				break;
			continue;
		}
		// printf the command type
		if (cmd_type != -1)
		{
			printf("Command: %s\n", CMD_STR[cmd_type]);
		}
		// handel differnt kinds of command
		switch (cmd_type)
		{
		case USER:
			n = respUSER(request, cfd, usr);
			break;
		case PASS:
			n = respPASS(request, cfd, usr);
			break;
		case RETR:
			n = respRETR(request, cfd, usr);
			break;
		case STOR:
			n = respSTOR(request, cfd, usr);
			break;
		case QUIT:
			n = respQUIT(request, cfd, usr);
			break;
		case ABOR:
			n = respABOR(request, cfd, usr);
			break;
		case SYST:
			n = respSYST(request, cfd, usr);
			break;
		case TYPE:
			n = respTYPE(request, cfd, usr);
			break;
		case PORT:
			n = respPORT(request, cfd, usr);
			break;
		case PASV:
			n = respPASV(request, cfd, usr);
			break;
		case MKD:
			n = respMKD(request, cfd, usr);
			break;
		case CWD:
			n = respCWD(request, cfd, usr);
			break;
		case PWD:
			n = respPWD(request, cfd, usr);
			break;
		case LIST:
			n = respLIST(request, cfd, usr);
			break;
		case RMD:
			n = respRMD(request, cfd, usr);
			break;
		case RNFR:
			n = respRNFR(request, cfd, usr);
			break;
		case RNTO:
			n = respRNTO(request, cfd, usr);
			break;
		case REST:
			n = respREST(request, cfd, usr);
			break;
		default:
			n = respWRONGREQ(cfd, usr);
			break;
		}
		if (n != 0)
			break;
		if (cmd_type != REST)
		{
			usr->position = 0;
		}
	}

	free(request);
	deleteUsr(usr);

	close(cfd);
	client_num--;
	resetThread(cfd);
	pthread_detach(pthread_self());
	return (void *)0;
}

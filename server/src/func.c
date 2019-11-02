#include "func.h"
#include "Usr.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>

extern char root_dir[256];
extern char server_ip[16];

// get request content
int getRequest(int cfd, char request[])
{
	int p = 0;
	while (1)
	{
		int n = read(cfd, request + p, 8191 - p);
		if (n < 0)
		{
			printf("Error read(): %s(%d)\n", strerror(errno), errno);
			return -1;
		}
		else if (n == 0)
		{
			break;
		}
		else
		{
			p += n;
			if (request[p - 1] == '\n')
			{
				break;
			}
		}
	}
	if (p >= 2 && request[p - 2] == '\r')
	{
		request[p - 2] = '\0';
	}
	if (p >= 1 && request[p - 1] == '\n')
	{
		request[p - 1] = '\0';
	}
	return 0;
}

// write response to the client
int writeResponse(int cfd, char buffer[], int len)
{
	int p = 0;
	int length = (len == -1) ? strlen(buffer) : len;
	while (p < length)
	{
		int n = send(cfd, buffer + p, length - p, MSG_NOSIGNAL);
		if (n < 0)
		{
			printf("Error write(): %s(%d)\n", strerror(errno), errno);
			return -1;
		}
		else
		{
			p += n;
		}
	}
	return 0;
}

// get parameters from request. if -1 error
int getParam(char request[], char buffer[], int buffer_size)
{
	int param_num = 1;
	int len = strlen(request);
	int index = -1;
	for (int i = 0; i < len; ++i)
	{
		if (request[i] == ' ')
		{
			index = i;
			param_num++;
		}
	}
	// more than need
	if (param_num != 2)
	{
		buffer[0] = '\0';
		return -1;
	}
	else
	{
		// last char = ' '
		if (index == len - 1)
		{
			buffer[0] = '\0';
			return -1;
		}
		else
		{
			// too long
			if (len - index > buffer_size)
			{
				buffer[0] = '\0';
				return -1;
			}
			strcpy(buffer, request + index + 1);
			return 0;
		}
	}
}

// get IP and port from buffer string
int getIPandPort(char buffer[], Usr *usr)
{
	char tmp[100] = "";
	strcpy(tmp, buffer); // buffer may change thus duplicate a copy
	int arr[6] = {0};
	int len = strlen(tmp);
	int dig_num = 0;
	int num = 0;
	int key_index = -1;
	for (int i = 0; i < len; ++i)
	{
		if (tmp[i] >= '0' && tmp[i] <= '9')
		{
			num = num * 10 + tmp[i] - '0';
		}
		else if (tmp[i] == ',')
		{
			tmp[i] = '.';
			if (num >= 256)
			{
				return -1; // out of legal range
			}
			arr[dig_num] = num;
			num = 0;
			dig_num++;
			if (dig_num == 6)
			{ // more ',' than need
				return -1;
			}
			if (dig_num == 4)
			{
				key_index = i;
			}
		}
		else
		{
			return -1;
		}
	}
	if (dig_num != 5 || tmp[len - 1] == ',' || num >= 256)
	{ // extra position
		return -1;
	}
	else
	{
		arr[5] = num;
		usr->cdata_port = arr[4] * 256 + arr[5];
		memset(usr->ip_addr, 0, sizeof(usr->ip_addr));
		strncpy(usr->ip_addr, tmp, key_index);
		return 0;
	}
}

// anonymous login
int respUSER(char request[], int cfd, Usr *usr)
{
	char buffer[100] = "";
	// check command
	if (strcmp(request, "USER anonymous") != 0)
	{
		strcpy(buffer, "501 Invalid parameters or formats.\r\n");
	}
	else
	{
		if (usr->state != NLOGIN)
		{
			if (usr->port_mode == 0)
			{
				usr->port_mode = -1;
				close(usr->slis_fd);
			}
			clearUsr(usr);
		}
		usr->state = NPWD;
		strcpy(buffer, "331 Guest login OK, send your complete e-maiil address as password.\r\n");
	}
	return writeResponse(cfd, buffer, -1);
}

// enter email as password
int respPASS(char request[], int cfd, Usr *usr)
{
	// longest e-mail :320 char.
	char buffer[384] = "";
	// first handle illegal parameters!
	int n = getParam(request, buffer, 384);
	if (n == -1)
	{
		strcpy(buffer, "501 Invalid parameters or formats.\r\n");
	}
	else
	{
		if (usr->state == NLOGIN)
		{
			strcpy(buffer, "503 Bad sequence of commands.\r\n");
		}
		else if (usr->state == NPWD)
		{
			usr->state = HLOGIN;
			strcpy(buffer, "230-welcome to mini FTP\r\n230 Guest login ok, access restrictions apply.\r\n");
		}
		else
		{
			usr->state = HLOGIN;
			strcpy(buffer, "503 Bad sequence of commands.\r\n");
		}
	}
	return writeResponse(cfd, buffer, -1);
}

// port mode, not build cnt
int respPORT(char request[], int cfd, Usr *usr)
{
	char buffer[100] = "";
	// check login
	if (usr->state == NLOGIN || usr->state == NPWD)
	{
		strcpy(buffer, "530 Login first please.\r\n");
		return writeResponse(cfd, buffer, -1);
	}

	usr->state = HLOGIN;
	// handle the illegal parameters
	int n1 = getParam(request, buffer, 100);
	int n2 = n1 == -1 ? -1 : getIPandPort(buffer, usr);
	if (n2 == -1)
	{
		strcpy(buffer, "501 Invalid parameters or formats.\r\n");
		return writeResponse(cfd, buffer, -1);
	}

	//  discard old connection
	if (usr->port_mode == 0)
		close(usr->slis_fd);
	usr->port_mode = 1;
	strcpy(buffer, "200 PORT command succeeded.\r\n");
	return writeResponse(cfd, buffer, -1);
}

// passive mode
int respPASV(char request[], int cfd, Usr *usr)
{
	char buffer[100] = "";
	// handle incorrect state
	if (usr->state == NLOGIN || usr->state == NPWD)
	{
		strcpy(buffer, "530 Login first please.\r\n");
		return writeResponse(cfd, buffer, -1);
	}

	usr->state = HLOGIN;
	// handle illegal parameters
	if (strcmp(request, "PASV") != 0)
	{
		strcpy(buffer, "501 Invalid parameters or formats.\r\n");
		return writeResponse(cfd, buffer, -1);
	}

	// get IP address of the server.
	char addr_str[100];
	strcpy(addr_str, server_ip);
	int addr_len = strlen(addr_str);
	for (int i = 0; i < addr_len; ++i)
	{
		if (addr_str[i] == '.')
		{
			addr_str[i] = ',';
		}
	}

	// close old connection
	if (usr->port_mode == 0)
	{
		close(usr->slis_fd);
		usr->port_mode = -1;
	}

	// bind port
	int lfd;
	struct sockaddr_in addr;
	int lfd_port = rand() % (65535 - 20000 + 1) + 20000;
	if ((lfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		strcpy(buffer, "451 Local error in processing.\r\n");
		return writeResponse(cfd, buffer, -1);
	}
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(lfd_port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
		strcpy(buffer, "451 Local error in processing.\r\n");
		return writeResponse(cfd, buffer, -1);
	}
	if (listen(lfd, 10) == -1)
	{
		printf("Error listen(): %s(%d)\n", strerror(errno), errno);
		strcpy(buffer, "451 Local error in processing.\r\n");
		return writeResponse(cfd, buffer, -1);
	}
	// reply message
	int p1 = lfd_port / 256;
	int p2 = lfd_port % 256;
	sprintf(buffer, "227 Entering Passive Mode(%s,%d,%d)\r\n", addr_str, p1, p2);
	usr->port_mode = 0;
	usr->slis_fd = lfd;
	return writeResponse(cfd, buffer, -1);
}

// build port connection
int buildPORTcnt(char buffer[], Usr *usr)
{
	struct sockaddr_in addr;
	// create socket
	if ((usr->sdata_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("Error buildPORTcnt socket(): %s(%d)\n", strerror(errno), errno);
		strcpy(buffer, "425 Connection attempt failed.\r\n");
		usr->port_mode = -1;
		return -1;
	}
	// client port and ip
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(usr->cdata_port);
	if (inet_pton(AF_INET, usr->ip_addr, &addr.sin_addr) <= 0)
	{
		printf("Error buildPORTcnt inet_pton(): %s(%d)\n", strerror(errno), errno);
		strcpy(buffer, "425 Connection attempt failed.\r\n");
		usr->port_mode = -1;
		return -1;
	}
	// connect dataport to client
	if (connect(usr->sdata_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		printf("Error buildPORTcnt connect(): %s(%d)\n", strerror(errno), errno);
		strcpy(buffer, "425 Connection attempt failed.\r\n");
		usr->port_mode = -1;
		return -1;
	}
	return 0;
}

// get data transfer fd
int getDatafd(char buffer[], Usr *usr)
{
	if (usr->port_mode == 1)
	{
		return buildPORTcnt(buffer, usr);
	}
	else if (usr->port_mode == 0)
	{
		if ((usr->sdata_fd = accept(usr->slis_fd, NULL, NULL)) == -1)
		{
			printf("Error getDatafd accept(): %s(%d)\n", strerror(errno), errno);
			strcpy(buffer, "425 Connection attempt failed.\r\n");
			usr->port_mode = -1;
			close(usr->slis_fd);
			return -1;
		}
		else
			return 0;
	}
	else
	{
		strcpy(buffer, "503 Bad sequence of commands.\r\n");
		return -1;
	}
}

// write data from local file
int write4File(char buffer[], Usr *usr, FILE *file)
{
	char sentence[8192] = "";
	memset(sentence, 0, 8192);
	int c = 0;
	while ((c = fread(sentence, 1, 8191, file)) > 0)
	{
		int n = writeResponse(usr->sdata_fd, sentence, c);
		// write error in transferring
		if (n == -1)
		{
			strcpy(buffer, "426 Data transfer connection broke.\r\n");
			return -1;
		}
	}
	if (!feof(file))
	{
		printf("Error write4File fets(): can't read local file.\n");
		strcpy(buffer, "451 Local error in processing.\r\n");
		return -1;
	}
	return 0;
}

// write data to local file
int write2File(char buffer[], Usr *usr, FILE *file)
{
	char sentence[8192] = "";
	memset(sentence, 0, 8192);
	int n = -1;
	while ((n = read(usr->sdata_fd, sentence, 8191)) > 0)
	{
		if (fwrite(sentence, 1, n, file) != n)
		{
			printf("Error write2File fputs() : can't wirte to file.\n");
			strcpy(buffer, "451 Local error in processing.\r\n");
			return -1;
		}
	}
	if (n < 0)
	{
		strcpy(buffer, "426 Data transfer connection broke.\r\n");
		return -1;
	}
	return 0;
}

// check share by RETR and STOR
void checkStateForRETRandPORT(char request[], char buffer[], char *dir, int dir_size, Usr *usr)
{
	// check state
	if (usr->state == NLOGIN || usr->state == NPWD)
	{
		strcpy(buffer, "530 Login first please.\r\n");
	}
	// check mode
	else if (usr->port_mode == -1)
	{
		usr->state = HLOGIN;
		strcpy(buffer, "503 Bad sequence of commands.\r\n");
	}
	else
	{
		usr->state = HLOGIN;
		// parameters quantitiy or format error
		if (getParam(request, dir, dir_size) == -1)
		{
			strcpy(buffer, "501 Invalid parameters or formats.\r\n");
		}
		else
		{
			// don't permit './' or '../'
			if (strlen(dir) > 0 && dir[0] == '.')
			{
				strcpy(buffer, "501 Invalid parameters or formats.\r\n");
			}
			// change relative path to absolute path
			if (strlen(buffer) == 0 && strlen(dir) > 0 && dir[0] != '/')
			{
				int actual_len = strlen(dir) + strlen(usr->root_dir) + 2;
				if (actual_len > 4096)
				{
					strcpy(buffer, "501 Invalid parameters or formats.\r\n");
				}
				else
				{
					char *mid = (char *)malloc(actual_len);
					memset(mid, 0, actual_len);
					sprintf(mid, "%s/%s", usr->root_dir, dir);
					strcpy(dir, mid);
					free(mid);
				}
			}
		}
	}
}

// retrieve file from server
int respRETR(char request[], int cfd, Usr *usr)
{
	char buffer[64] = "";

	// use dir to get file directory
	int len = strlen(request) + strlen(usr->root_dir);
	int dir_size = len > 4096 ? 4096 : len;
	char *dir = (char *)malloc(dir_size);
	memset(dir, 0, dir_size);

	// some check shared by RETR and PORT, like state, parameters
	checkStateForRETRandPORT(request, buffer, dir, dir_size, usr);

	// check if file exist
	if (strlen(buffer) == 0 && access(dir, F_OK) != 0)
	{
		strcpy(buffer, "550 File not exist.\r\n");
	}

	// error has occurred
	if (strlen(buffer) != 0)
	{
		free(dir);
		return writeResponse(cfd, buffer, -1);
	}

	// get data transfer port, if PORT mode:build connection
	int n = getDatafd(buffer, usr);
	if (n == -1)
	{
		free(dir);
		return writeResponse(cfd, buffer, -1);
	}

	// send mark
	strcpy(buffer, "150 Start opening BINARY mode data connection.\r\n");
	if (writeResponse(cfd, buffer, -1) == -1)
	{
		free(dir);
		return -1;
	}

	//open file
	FILE *file = fopen(dir, "rb");
	if (file == NULL)
	{
		n = -1;
		strcpy(buffer, "550 can't open file.\r\n");
	}
	else
	{
		if (fseek(file, usr->position, SEEK_SET) != 0)
		{
			n = -1;
			strcpy(buffer, "502 error caused by previous rest.\r\n");
		}
		else
		{
			n = write4File(buffer, usr, file);
		}
	}

	// close file and port
	if (file != NULL)
		fclose(file);
	close(usr->sdata_fd);
	if (usr->port_mode == 0)
		close(usr->slis_fd);
	usr->port_mode = -1;

	// make response
	if (n == 0)
		strcpy(buffer, "226 transfer complete.\r\n");
	free(dir);
	return writeResponse(cfd, buffer, -1);
}

// store file in the server
int respSTOR(char request[], int cfd, Usr *usr)
{
	char buffer[64] = "";

	int len = strlen(request) + strlen(usr->root_dir);
	int dir_size = len > 4096 ? 4096 : len;
	char *dir = (char *)malloc(dir_size);
	memset(dir, 0, dir_size);

	// some check shared by RETR and PORT
	checkStateForRETRandPORT(request, buffer, dir, dir_size, usr);

	if (strlen(buffer) != 0)
	{
		free(dir);
		return writeResponse(cfd, buffer, -1);
	}

	// get data fd
	int n = getDatafd(buffer, usr);
	if (n == -1)
	{
		free(dir);
		return writeResponse(cfd, buffer, -1);
	}

	// send mark
	strcpy(buffer, "150 Start opening BINARY mode data connection.\r\n");
	if (writeResponse(cfd, buffer, -1) == -1)
	{
		free(dir);
		return -1;
	}

	FILE *file = fopen(dir, "wb");
	if (file == NULL)
	{
		n = -1;
		strcpy(buffer, "553 can't create file.\r\n");
	}
	else
	{
		n = write2File(buffer, usr, file);
	}

	// close file and port
	if (file != NULL)
		fclose(file);
	close(usr->sdata_fd);
	if (usr->port_mode == 0)
		close(usr->slis_fd);
	usr->port_mode = -1;

	// return message
	if (n == 0)
		strcpy(buffer, "226 transfer complete.\r\n");
	free(dir);
	return writeResponse(cfd, buffer, -1);
}

int send_list(char *dir, char *buffer, Usr *usr, int is_dir)
{
	int cmd_size = strlen(dir) + 10;
	char *cmd = (char *)malloc(cmd_size);
	memset(cmd, 0, cmd_size);
	strcat(cmd, "ls -l ");
	strcat(cmd, dir);
	FILE *pipe = popen(cmd, "r");
	free(cmd);
	if (pipe == NULL)
	{
		strcpy(buffer, "550 Local directory or file error.\r\n");
		return -1;
	}
	char *tmp = (char *)malloc(4196);
	memset(tmp, 0, 4196);
	int line = is_dir;
	while (fgets(tmp, 4196, pipe))
	{
		if (line == 0)
		{
			line++;
			continue;
		}
		int s_len = strlen(tmp);
		if (tmp[s_len - 1] == '\n')
		{
			tmp[s_len - 1] = '\0';
			strcat(tmp, "\r\n");
		}
		else
			continue;
		int n = writeResponse(usr->sdata_fd, tmp, -1);
		if (n == -1)
		{
			free(tmp);
			strcpy(buffer, "426 Data transfer connection broke.\r\n");
			return -1;
		}
	}
	pclose(pipe);
	free(tmp);
	return 1;
}

// printf list of current directory
int respLIST(char request[], int cfd, Usr *usr)
{
	char buffer[64] = "";
	if (usr->state == NLOGIN || usr->state == NPWD)
	{
		strcpy(buffer, "530 Login first please.\r\n");
	}
	else if (usr->port_mode == -1)
	{
		usr->state = HLOGIN;
		strcpy(buffer, "503 Bad sequence of commands.\r\n");
	}
	else
	{
		usr->state = HLOGIN;
	}

	if (strlen(buffer) != 0)
	{
		return writeResponse(cfd, buffer, -1);
	}

	// get dir, absolute or relative
	char dir[4096] = "";
	memset(dir, 0, 4096);
	if (strcmp(request, "LIST") == 0)
	{
		strcpy(dir, usr->root_dir);
	}
	else
	{
		int len = strlen(request);
		char *mid = (char *)malloc(len);
		memset(mid, 0, len);
		if (getParam(request, mid, len) == -1)
		{
			strcpy(buffer, "501 Invalid parameters or formats.\r\n");
			free(mid);
			return writeResponse(cfd, buffer, -1);
		}
		else
		{
			if (strlen(mid) >= 1 && mid[0] == '.')
			{
				strcpy(buffer, "501 Invalid parameters or formats.\r\n");
				free(mid);
				return writeResponse(cfd, buffer, -1);
			}
			else if (strlen(mid) >= 1 && mid[0] != '/')
			{
				int actual_len = strlen(usr->root_dir) + strlen(mid) + 2;
				if (actual_len > 4096)
				{
					strcpy(buffer, "501 Invalid parameters or formats.\r\n");
					free(mid);
					return writeResponse(cfd, buffer, -1);
				}
				else
				{
					sprintf(dir, "%s/%s", usr->root_dir, mid);
				}
			}
			else
			{
				strcpy(dir, mid);
			}
			free(mid);
		}
	}

	// directory or file is ok.
	DIR *dp = opendir(dir);
	int n = access(dir, F_OK);
	int is_dir = 1;
	if (dp == NULL && n != 0)
	{
		strcpy(buffer, "550 Directory or file doesn't exist.\r\n");
		return writeResponse(cfd, buffer, -1);
	}
	if (dp != NULL)
	{
		is_dir = 0;
		closedir(dp);
	}

	//get data fd
	n = getDatafd(buffer, usr);
	if (n == -1)
		return writeResponse(cfd, buffer, -1);

	// send mark
	strcpy(buffer, "150 Start opening BINARY mode data connection.\r\n");
	n = writeResponse(cfd, buffer, -1);
	if (n == -1)
		return -1;

	// send list to client
	n = send_list(dir, buffer, usr, is_dir);

	close(usr->sdata_fd);
	if (usr->port_mode == 0)
		close(usr->slis_fd);
	usr->port_mode = -1;
	if (n != -1)
	{
		strcpy(buffer, "226 Transfer complete.\r\n");
	}

	return writeResponse(cfd, buffer, -1);
}

// system info of the server
int respSYST(char request[], int cfd, Usr *usr)
{
	char buffer[64] = "";
	// set state every time
	if (usr->state > NPWD)
	{
		usr->state = HLOGIN;
	}
	if (strcmp(request, "SYST") != 0)
	{
		strcpy(buffer, "504 no parameters are needed!\r\n");
	}
	else
	{
		strcpy(buffer, "215 UNIX Type: L8\r\n");
	}
	return writeResponse(cfd, buffer, -1);
}

// ftp transfer type
int respTYPE(char request[], int cfd, Usr *usr)
{
	char buffer[64] = "";
	if (usr->state == NLOGIN || usr->state == NPWD)
	{
		strcpy(buffer, "530 Login first please.\r\n");
	}
	else
	{
		usr->state = HLOGIN;
		int n = getParam(request, buffer, 64);
		if (n == -1 || strcmp(buffer, "I") != 0)
		{
			strcpy(buffer, "501 Invalid parameters or formats.\r\n");
		}
		else
		{
			strcpy(buffer, "200 Type set to I.\r\n");
		}
	}
	return writeResponse(cfd, buffer, -1);
}

// logout
int respQUIT(char request[], int cfd, Usr *usr)
{
	char buffer[64] = "";
	if (strcmp(request, "QUIT") != 0)
	{
		strcpy(buffer, "504 No parameters are needed!\r\n");
	}
	else
	{
		// close all port
		if (usr->port_mode == 0)
			close(usr->slis_fd);
		// close(usr->sdata_fd);
		strcpy(buffer, "221-Thank you for using the FTP service\r\n221 Goodbye.\r\n");
	}
	writeResponse(cfd, buffer, -1);
	return 1;
}

// same as quit
int respABOR(char request[], int cfd, Usr *usr)
{
	char buffer[64] = "";
	if (strcmp(request, "ABOR") != 0)
	{
		strcpy(buffer, "504 no parameters are needed!\r\n");
	}
	else
	{
		//Todo  close all port
		if (usr->port_mode == 0)
			close(usr->slis_fd);
		// close(usr->sdata_fd);
		strcpy(buffer, "221-Thank you for using the FTP service\r\n221 Goodbye.\r\n");
	}
	writeResponse(cfd, buffer, -1);
	return 1;
}

// change name prefix of current user
int respCWD(char request[], int cfd, Usr *usr)
{
	char buffer[100] = "";
	// handle state first
	if (usr->state == NLOGIN || usr->state == NPWD)
	{
		strcpy(buffer, "530 Login first please.\r\n");
	}
	else
	{
		// set state
		usr->state = HLOGIN;
		// maximum path legnth = 4096
		int len = strlen(request);
		int dir_size = 4096 > len ? len : 4096;
		char *dir = (char *)malloc(dir_size);
		memset(dir, '\0', dir_size);

		if (getParam(request, dir, dir_size) == -1)
		{
			strcpy(buffer, "501 Invalid parameters or formats.\r\n");
		}
		else
		{
			if (strlen(dir) > 0 && dir[0] != '/')
			{
				int actual_len = strlen(dir) + strlen(usr->root_dir) + 2;
				if (actual_len > 4096)
				{
					strcpy(buffer, "501 Invalid parameters or formats.\r\n");
				}
				else
				{
					char *mid = (char *)malloc(actual_len);
					memset(mid, 0, actual_len);
					sprintf(mid, "%s/%s", usr->root_dir, dir);
					strcpy(dir, mid);
					free(mid);
				}
			}
			if (strlen(buffer) == 0)
			{
				DIR *dp;
				if (strlen(dir) == 0 || (dp = opendir(dir)) == NULL)
				{
					strcpy(buffer, "550 No such file or directory!\r\n");
				}
				else
				{
					closedir(dp);
					strcpy(usr->root_dir, dir);
					strcpy(buffer, "250 Okay.\r\n");
				}
			}
		}
		free(dir);
	}
	return writeResponse(cfd, buffer, -1);
}

// print name prefix of current user
int respPWD(char request[], int cfd, Usr *usr)
{
	// need to consider the length of the dir
	int len = strlen(usr->root_dir) + 16;
	int buffer_size = len > 64 ? len : 64;
	char *buffer = (char *)malloc(buffer_size);
	memset(buffer, 0, buffer_size);

	// handle state
	if (usr->state == NLOGIN || usr->state == NPWD)
	{
		strcpy(buffer, "530 Login first please.\r\n");
	}
	else
	{
		usr->state = HLOGIN;
		// handle illegal parameters
		if (strcmp(request, "PWD") != 0)
		{
			strcpy(buffer, "504 no parameters are needed!\r\n");
		}
		else
		{
			sprintf(buffer, "257 \"%s\"\r\n", usr->root_dir);
		}
	}
	int n = writeResponse(cfd, buffer, -1);
	free(buffer);
	return n;
}

// make a directory, absolute or relative
int respMKD(char request[], int cfd, Usr *usr)
{
	// may need concat param and usr->root
	int len = strlen(request) + strlen(usr->root_dir) + 8;
	int buffer_size = len > 64 ? len : 64;
	char *buffer = (char *)malloc(buffer_size);
	memset(buffer, 0, buffer_size);

	// handle state
	if (usr->state == NLOGIN || usr->state == NPWD)
	{
		strcpy(buffer, "530 Login first please.\r\n");
	}
	else
	{
		usr->state = HLOGIN;

		int dir_size = buffer_size > 4096 ? 4096 : buffer_size;
		char *dir = (char *)malloc(dir_size);
		memset(dir, 0, dir_size);

		if (getParam(request, dir, dir_size) == -1)
		{
			strcpy(buffer, "501 Invalid parameters or formats.\r\n");
		}
		else
		{
			// change relative path to absolute path
			int cont = 1;
			if (strlen(dir) > 0 && dir[0] != '/')
			{
				int actual_len = strlen(dir) + strlen(usr->root_dir) + 2;
				if (actual_len > 4096)
				{
					cont = 0;
					strcpy(buffer, "501 Invalid parameters or formats.\r\n");
				}
				else
				{
					sprintf(buffer, "%s/%s", usr->root_dir, dir);
					strcpy(dir, buffer);
				}
			}
			// try make dir
			if (cont)
			{
				if (mkdir(dir, S_IRWXU) == -1)
				{
					strcpy(buffer, "550 directory creation failed.\r\n");
				}
				else
				{
					sprintf(buffer, "257 \"%s\"\r\n", dir);
				}
			}
		}
		free(dir);
	}

	int n = writeResponse(cfd, buffer, -1);
	free(buffer);
	return n;
}

int rmDirRecursively(char *dir_name, int dir_size)
{
	DIR *dir;
	struct dirent *entry;
	dir = opendir(dir_name);
	int dir_len = strlen(dir_name);
	if (dir == NULL)
	{
		printf("Error: open dir: %s\n", dir_name);
		return -1;
	}
	while ((entry = readdir(dir)) != NULL)
	{
		if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
		{
			// dir/subdir\0
			int total_len = dir_len + strlen(entry->d_name) + 2;
			if (total_len > dir_size)
			{
				printf("Error: delete dir: %s/%s\n", dir_name, entry->d_name);
				return -1;
			}
			else
			{
				strcat(dir_name, "/");
				strcat(dir_name, entry->d_name);
				if (entry->d_type == DT_DIR)
				{
					int n = rmDirRecursively(dir_name, dir_size);
					if (n == -1)
						return -1;
				}
				else
				{
					if (remove(dir_name) == -1)
					{
						printf("Error: delete file: %s\n", dir_name);
						return -1;
					}
				}
				dir_name[dir_len] = '\0';
			}
		}
	}
	closedir(dir);
	if (rmdir(dir_name) == -1)
	{
		printf("Error: remove dir: %s\n", dir_name);
		return -1;
	}
	return 0;
}

// delete a directory, absolute or relative
int respRMD(char request[], int cfd, Usr *usr)
{
	char buffer[64] = "";
	// handle state
	if (usr->state == NLOGIN || usr->state == NPWD)
	{
		strcpy(buffer, "530 Login first please.\r\n");
	}
	else
	{
		usr->state = HLOGIN;
		// malloc space for dir
		int dir_size = 4096;
		char *dir = (char *)malloc(dir_size);
		memset(dir, '\0', dir_size);

		if (getParam(request, dir, dir_size) == -1)
		{
			strcpy(buffer, "501 Invalid parameters or formats.\r\n");
		}
		else
		{
			if (strlen(dir) > 0 && dir[0] != '/')
			{
				int actual_len = strlen(dir) + strlen(usr->root_dir) + 2;
				if (actual_len > dir_size)
				{
					strcpy(buffer, "501 Invalid parameters or formats.\r\n");
				}
				else
				{
					char *mid = (char *)malloc(actual_len);
					memset(mid, '\0', actual_len);
					sprintf(mid, "%s/%s", usr->root_dir, dir);
					strcpy(dir, mid);
					free(mid);
				}
			}
			if (strlen(buffer) == 0)
			{
				if (rmDirRecursively(dir, dir_size) == -1)
				{
					strcpy(buffer, "550 direcotry removal failed.\r\n");
				}
				else
				{
					strcpy(buffer, "250 Directory removal succeeded.\r\n");
				}
			}
		}
		free(dir);
	}
	return writeResponse(cfd, buffer, -1);
}

// rename from a file name
int respRNFR(char request[], int cfd, Usr *usr)
{
	char buffer[64] = "";
	if (usr->state == NLOGIN || usr->state == NPWD)
	{
		strcpy(buffer, "530 Login first please.\r\n");
	}
	else
	{
		usr->state = HLOGIN;

		int len = strlen(request) + strlen(usr->root_dir);
		int dir_size = len > 4096 ? 4096 : len;
		char *dir = (char *)malloc(dir_size);

		if (getParam(request, dir, dir_size) == -1)
		{
			strcpy(buffer, "501 Invalid parameters or formats.\r\n");
		}
		else
		{
			int cont = 1;
			// change relative path to absolute path
			if (strlen(dir) > 0 && dir[0] != '/')
			{
				int actual_len = strlen(usr->root_dir) + strlen(dir) + 2;
				if (actual_len > 4096)
				{
					cont = 0;
					strcpy(buffer, "501 Invalid parameters or formats.\r\n");
				}
				else
				{
					char *mid = (char *)malloc(dir_size);
					memset(mid, '\0', dir_size);
					sprintf(mid, "%s/%s", usr->root_dir, dir);
					strcpy(dir, mid);
					free(mid);
				}
			}
			if (cont)
			{
				if (access(dir, F_OK) == 0)
				{
					usr->state = RNAME;
					strcpy(usr->old_dir, dir);
					strcpy(buffer, "350 file exists.\r\n");
				}
				else
				{
					strcpy(buffer, "550 File not found.\r\n");
				}
			}
		}

		free(dir);
	}
	return writeResponse(cfd, buffer, -1);
}

// rename to a given name
int respRNTO(char request[], int cfd, Usr *usr)
{
	char buffer[64] = "";
	if (usr->state == NLOGIN || usr->state == NPWD)
	{
		strcpy(buffer, "530 Login first please.\r\n");
	}
	else if (usr->state != RNAME)
	{
		strcpy(buffer, "503 Bad sequence of commands.\r\n");
	}
	else
	{
		usr->state = HLOGIN;

		// malloc space for dir
		int len = strlen(request) + strlen(usr->root_dir);
		int dir_size = len > 4096 ? 4096 : len;
		char *dir = (char *)malloc(dir_size);
		memset(dir, '\0', dir_size);

		if (getParam(request, dir, dir_size) == -1)
		{
			strcpy(buffer, "501 Invalid parameters or formats.\r\n");
		}
		else
		{
			int cont = 1;
			// convert relative path to absolute path
			if (strlen(dir) > 0 && dir[0] != '/')
			{
				int actual_len = strlen(dir) + strlen(usr->root_dir) + 2;
				if (actual_len > 4096)
				{
					cont = 0;
					strcpy(buffer, "501 Invalid parameters or formats.\r\n");
				}
				else
				{
					char *mid = (char *)malloc(actual_len);
					sprintf(mid, "%s/%s", usr->root_dir, dir);
					strcpy(dir, mid);
					free(mid);
				}
			}
			if (cont)
			{
				// try rename
				if (rename(usr->old_dir, dir) == -1)
				{
					strcpy(buffer, "553 Rename file failed.\r\n");
				}
				else
				{
					strcpy(buffer, "250 File was renamed successfully.\r\n");
				}
				usr->old_dir[0] = '\0';
			}
		}
		free(dir);
	}
	return writeResponse(cfd, buffer, -1);
}

int respREST(char request[], int cfd, Usr *usr)
{
	char buffer[64] = "";
	if (usr->state == NLOGIN || usr->state == NPWD)
	{
		strcpy(buffer, "530 Login first please.\r\n");
		return writeResponse(cfd, buffer, -1);
	}
	else
	{
		usr->state = HLOGIN;
		int n = getParam(request, buffer, 64);
		if (n == -1)
		{
			strcpy(buffer, "501 Invalid parameters or formats.\r\n");
			return writeResponse(cfd, buffer, -1);
		}
		else
		{
			int position = atoi(buffer);
			if (strcmp(buffer, "0") != 0 && position == 0)
			{
				strcpy(buffer, "501 Invalid parameters or formats.\r\n");
				return writeResponse(cfd, buffer, -1);
			}
			else
			{
				usr->position = position;
				strcpy(buffer, "350 rest succeeded.\r\n");
				return writeResponse(cfd, buffer, -1);
			}
		}
	}
}

// command not recognized
int respWRONGREQ(int cfd, Usr *usr)
{
	char buffer[64] = "";
	if (usr->state > NPWD)
	{
		usr->state = HLOGIN;
	}
	strcpy(buffer, "500 Command unrecognized.\r\n");
	return writeResponse(cfd, buffer, -1);
}

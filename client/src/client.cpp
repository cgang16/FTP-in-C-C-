#include "func.h"
#include "Usr.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <istream>
#include <iostream>


static char SERVER_IP[16] = "127.0.0.1";
static int SERVER_PORT = 21;
typedef enum { USER, PASS, RETR, STOR, QUIT, ABOR, SYST, TYPE, PORT, PASV, MKD, CWD, PWD, LIST, RMD, RNFR, RNTO }CMD_TYPE;
static char* CMD_STR[] = { "USER", "PASS", "RETR", "STOR", "QUIT", "ABOR", "SYST", "TYPE", "PORT", "PASV", "MKD", "CWD",
					"PWD", "LIST", "RMD", "RNFR", "RNTO" };

char work_dir[256] = "/tmp";


// get command line argument
int getCommandLineArg(int argc, char** argv);

int getCmdtype(char request[]);

int main(int argc, char**argv){

    // get server IP and port
    getCommandLineArg(argc, argv);

    int conn_fd;
    // create socket
    if((conn_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1){
        printf("Error socket(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    
    if(inet_pton(AF_INET, SERVER_IP, &addr.sin_addr) <= 0){
        printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }

    if(connect(conn_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0 ){
        printf("Error connect(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    
    char sentence[8192] = "";
    memset(sentence, 0, 8192);

    if(readResponse(conn_fd, sentence) == -1){
        printf("Error: read initial response from server failed.\n");
        return 1;
    }
    else{
        printf("%s\n", sentence);
    }

    Usr* usr = (Usr*)malloc(sizeof(Usr));
    initialUsr(usr);

    while(true){
        printf("ftp> ");

        // get input contents and flush the stdin
        std::cin.getline(sentence, 8190, '\n');
        fflush(stdin);

        // handle illegal command.
        int n = getCmdtype(sentence);
        if(n == -1){
            printf("? Invalid command.\n");
            continue;
        }
        bool arg_right = isLegalArg(sentence); 
        strcat(sentence, "\r\n");
        writeRequest(conn_fd, sentence, -1);
        readResponse(conn_fd, sentence);
        printf("%s\n", sentence);
    } 

    close(conn_fd);
    return 0;
}

//get command line argument
int getCommandLineArg(int argc, char** argv){
	for(int i = 1; i < argc; ++i){
		if(strcmp(argv[i], "-ip") == 0){
			if(i+1 >= argc){
				printf("Error: Need IP address of the server.\n");
				return 1;
			}
            struct sockaddr_in addr;
            if(inet_pton(AF_INET, argv[i+1], &addr.sin_addr) <= 0){
                printf("Error: Please input a legal IP address.\n");
                return 1;
            }
			strcpy(SERVER_IP, argv[++i]);
		}
		else if(strcmp(argv[i], "-port") == 0){
			if(i+1 >= argc){
				printf("Error: need port number.\n");
				return 1;
			}
			SERVER_PORT = atoi(argv[++i]);
			if(SERVER_PORT == 0){
				printf("Error: Please input a legal port number.\n");
				return 1;
			}
		}
		else{
			printf("Error: Illegal command line arguments.\n");
			return 1;
		}
	}
	return 0;
}

// get command type 
int getCmdtype(char request[]) {
	for (int i = 0; i < 17; ++i) {
		if (strncmp(request, CMD_STR[i], strlen(CMD_STR[i])) == 0) {
			return i;
		}
	}
	return -1;
}
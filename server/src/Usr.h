#ifndef USR_H
#define USR_H

// need login, need password, have logged in, renmae state
typedef enum { NLOGIN, NPWD, HLOGIN, RNAME }USER_STATE;

typedef struct Usr {
	USER_STATE state;  // user's state

	int port_mode;     // 1 -- port mode 0 -- pasv mode
	
	int cdata_port;    // port mode ip address and data port
	char ip_addr[16];  

	int slis_fd;       // pasv mode listenfd = slfd, port = slfd_port    

	int sdata_fd;      // server_datafd, to transfer data

	char* root_dir;    // user's root dir

	char* old_dir;     // rename cmd old dir

	int position;      // for rest command.
}Usr;

// initialize user, malloc space
void initializeUsr(Usr* usr);

// clear user, clear some info
void clearUsr(Usr* usr);

// delete user, free space
void deleteUsr(Usr* usr);

#endif
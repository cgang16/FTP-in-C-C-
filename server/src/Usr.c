#include "Usr.h"
#include "stdlib.h"
#include "string.h"

extern char root_dir[256];

// initialize user , malloc space for some vars
void initializeUsr(Usr* usr){
	usr->state = NLOGIN;
	usr->port_mode = -1;
	usr->cdata_port = -1;
	usr->slis_fd = -1;
	usr->sdata_fd = -1;
	usr->position = 0;
	memset(usr->ip_addr, 0, 16);
	usr->root_dir = (char*)malloc(4096);
	memset(usr->root_dir, 0, 4096);
	usr->old_dir = (char*)malloc(4096);
	memset(usr->old_dir, 0, 4096);
	strcpy(usr->root_dir, root_dir);
}

// reset the user, clear some info
void clearUsr(Usr* usr){
	usr->state = NLOGIN;
	usr->port_mode = -1;
	usr->position = 0;
	memset(usr->root_dir, 0, 4096);
	memset(usr->old_dir, 0, 4096);
	strcpy(usr->root_dir, root_dir);
}

// delete user, free space
void deleteUsr(Usr* usr){
	free(usr->root_dir);
	free(usr->old_dir);
	free(usr);
}

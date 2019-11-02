#ifndef FUNC_H
#define FUNC_H

#include "Usr.h"
#include <stdio.h>

// get request content
int getRequest(int cfd, char request[]);

// write response to the client
int writeResponse(int cfd, char buffer[], int len);

// get parameters and store it in buffer, error return -1
int getParam(char request[], char buffer[], int buffer_size);

// get IP and Port from buffer and store it in user's member var
int getIPandPort(char buffer[], Usr *usr);

int respUSER(char request[], int cfd, Usr *usr);

int respPASS(char request[], int cfd, Usr *usr);

int respPORT(char request[], int cfd, Usr *usr);

int respPASV(char request[], int cfd, Usr *usr);

// build port connection
int buildPORTcnt(char buffer[], Usr *usr);

// get data transfer fd
int getDatafd(char buffer[], Usr *usr);

// write from local file
int write4File(char buffer[], Usr *usr, FILE *file);

// write to local file
int write2File(char buffer[], Usr *usr, FILE *file);

// check shared by RETR and PORT
void checkStateForRETRandPORT(char request[], char buffer[], char *dir, int dir_size, Usr *usr);

int respRETR(char request[], int cfd, Usr *usr);

int respSTOR(char request[], int cfd, Usr *usr);

int send_list(char *dir, char *buffer, Usr *usr, int is_dir);

int respLIST(char request[], int cfd, Usr *usr);

int respSYST(char request[], int cfd, Usr *usr);

int respTYPE(char request[], int cfd, Usr *usr);

int respQUIT(char request[], int cfd, Usr *usr);

int respABOR(char request[], int cfd, Usr *usr);

int respCWD(char request[], int cfd, Usr *usr);

int respPWD(char request[], int cfd, Usr *usr);

int respMKD(char request[], int cfd, Usr *usr);

int respRMD(char request[], int cfd, Usr *usr);

int respRNFR(char request[], int cfd, Usr *usr);

int respRNTO(char request[], int cfd, Usr *usr);

int respREST(char request[], int cfd, Usr *usr);

int respWRONGREQ(int cfd, Usr *usr);

#endif

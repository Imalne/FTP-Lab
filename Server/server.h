//
// Created by whale on 18-10-26.
//

#ifndef SERVERDEMO_SERVER_H
#define SERVERDEMO_SERVER_H
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <unistd.h>
#include <errno.h>

#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include <malloc.h>
#include <regex.h>
#include <stdlib.h>
#include <netdb.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <aio.h>
#include <dirent.h>


#define IPLENGTH 16
#define IP_SIZE  16
#define BUFLIMIT 8191
#define BUFLENGTH 8192

struct Client{
    int connfd;
    char sentence[8192];
    char ret[8192];
    char ClientIP[IPLENGTH];
    int ClientPort;
    int p;
    int login;
    char cwd[256];
    char lastCMD[BUFLIMIT];
    char fileSelected[BUFLIMIT];
    int listenfd;
    int data_socket;
    int offset;
    int appe;
    int in_loading;
};

struct loadMsg{
    int fd;
    int datafd;
    struct Client* client;
};


void toUpper(char* p);

int GetCmdFromClient(int connfd,char* cmd,int MAXSIZE);

int WriteToClient(int connfd,char* ret,int len);

char* scrach(char *s);

int CheckNumString(char* p);

int CharToInt(char *p);

int USER_Handler(struct Client* client);

int PASS_Handler(struct Client* client);

int PORT_Handler(struct Client* client);

int PASV_Handler(struct Client* client);

int SYST_Handler(struct Client* client);

int TYPE_Handler(struct Client* client);

int PWD_Handler(struct Client* client);

int CWD_Handler(struct Client* client);

int MKD_Handler(struct Client* client);

int RMD_Handler(struct Client* client);

int LIST_Handler(struct Client* client);

int RNFR_Handler(struct Client* client);

int RNTO_Handler(struct Client* client);

int QUIT_Handler(struct Client* client);

int RETR_Handler(struct Client* client);

int STOR_Handler(struct Client* client);

int REST_Handler(struct Client* client);

int APPE_Handler(struct Client* client);


extern const char * split;
extern int PORT;
extern char DIREC[256];


#endif //SERVERDEMO_SERVER_H

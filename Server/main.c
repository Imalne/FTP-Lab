#include "server.h"
int MAX_LINK = 10;
int linkNum = 0;

char * getRequest = "220 Anonymous FTP server ready.\r\n";


void reset_after_handle(struct Client* client,char* cmd){
    if(!(!strcmp(cmd,"RETR") || !strcmp(cmd,"REST"))){
        client->offset = 0;
        printf("%s",cmd);
    }
}

//cmd:operation
//org:sentence
int Distributor(struct Client* client,char* cmd){
    if(client->in_loading){
        strcpy(client->ret,"404 in data transfer\r\n");
        return 0;
    }
    toUpper(cmd);
    if (!strcmp(cmd,"USER")){
        USER_Handler(client);
        strcpy(client->lastCMD,cmd);
    }
    else if (!strcmp(cmd,"PASS"))
    {
        PASS_Handler(client);
        strcpy(client->lastCMD,cmd);
    }
    else if (!strcmp(cmd,"PORT"))
    {
        PORT_Handler(client);
//        strcpy(ret,"Command not complete");
        strcpy(client->lastCMD,cmd);

    }
    else if (!strcmp(cmd,"PASV"))
    {
        PASV_Handler(client);
//        strcpy(ret,"Command not complete");

        strcpy(client->lastCMD,cmd);

    }
    else if(!strcmp(cmd,"SYST")){
        SYST_Handler(client);

        strcpy(client->lastCMD,cmd);

    }
    else if(!strcmp(cmd,"TYPE")){
        TYPE_Handler(client);
        strcpy(client->lastCMD,cmd);

    }
    else if(!strcmp(cmd,"PWD")){
        PWD_Handler(client);
        strcpy(client->lastCMD,cmd);

    }
    else if(!strcmp(cmd,"CWD")){
        CWD_Handler(client);
        strcpy(client->lastCMD,cmd);

    }
    else if(!strcmp(cmd,"MKD")){
        MKD_Handler(client);
        strcpy(client->lastCMD,cmd);

    }
    else if(!strcmp(cmd,"RMD")){
        RMD_Handler(client);
        strcpy(client->lastCMD,cmd);

    }
    else if(!strcmp(cmd,"LIST")){
        LIST_Handler(client);
        strcpy(client->lastCMD,cmd);

    }
    else if(!strcmp(cmd,"RNFR")){
        RNFR_Handler(client);
        strcpy(client->lastCMD,cmd);

    }
    else if(!strcmp(cmd,"RNTO")){
        RNTO_Handler(client);
        strcpy(client->lastCMD,cmd);

    }
    else if(!strcmp(cmd,"QUIT")||!strcmp(cmd,"ABOR")){
        QUIT_Handler(client);
        strcpy(client->lastCMD,cmd);

    }
    else if(!strcmp(cmd,"RETR")){
        RETR_Handler(client);
        strcpy(client->lastCMD,cmd);
    }
    else if(!strcmp(cmd,"STOR")){
        STOR_Handler(client);
        strcpy(client->lastCMD,cmd);

    }
    else if(!strcmp(cmd,"REST")){
        REST_Handler(client);
        strcpy(client->lastCMD,cmd);

    }
    else if(!strcmp(cmd,"APPE")){
        APPE_Handler(client);
        strcpy(client->lastCMD,cmd);
    }
    else{
    strcpy(client->ret,"404 Command no Handler\r\n");
    }
    reset_after_handle(client,cmd);
    return 0;
}



//int handler(int connfd,char *cmd,int len,char *ret,int MAXSIZE,int *login,char* ip,int* port,char* cwd,char* lastCMD,char* fileSelected,int* listenfd,int* data_socket) {
int handler(struct Client * client,int len) {

    char *op = scrach(client->sentence);
    memset(client->ret, 0, (size_t) BUFLENGTH);
    if (op == NULL){
        strcpy(client->ret, "command not found");
        return 1;
    }
    Distributor(client,op);
    //free(op);
    return 0;
}


void* StartContorlLink(void* args){
    struct Client client;
    client.connfd = *((int*)args);
    client.ClientPort = 0;
    client.p = 0;
    client.login = 0;
    strcpy(client.cwd,"/tmp");
    client.listenfd = 0;
    client.data_socket = 0;
    client.appe = 0;
    client.in_loading = 0;

    if(WriteToClient(client.connfd,getRequest,33)>0)
    {
        printf("connect error");
        close(client.connfd);
        pthread_exit(NULL);
    };
    while(1) {

        int len = GetCmdFromClient(client.connfd,client.sentence,BUFLIMIT);

        if(len == 0){
            break;
        }

        handler(&client,len);
        if(WriteToClient(client.connfd, client.ret, (int) strlen(client.ret)) > 0)
        {
            break;
        };
    }
    close(client.connfd);
    pthread_exit(NULL);
}



int main(int argc, char **argv) {
    int listenfd, connfd;
    struct sockaddr_in addr;
    pthread_t tids[MAX_LINK];

    if(argc >1){
        strcpy(DIREC,argv[4]);
        PORT = atoi(argv[2]);
    }

    if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        printf("Error socket(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }


    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t) PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        printf("Error bind(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }

    if (listen(listenfd, MAX_LINK) == -1) {
        printf("Error listen(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }

    while (1) {
        if ((connfd = accept(listenfd, NULL, NULL)) == -1)
        {
            printf("Error accept(): %s(%d)\n", strerror(errno), errno);
            continue;
        }

        int ret = pthread_create(&tids[linkNum], NULL, StartContorlLink, (void*)&connfd );
        if( ret != 0 )
        {
            printf("pthread_create error:error_code= %d\n",ret);
        }
    }
    close(listenfd);
}

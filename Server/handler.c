//
// Created by whale on 18-10-26.
//


#include "server.h"

const char * split = " ,";

int PORT=21;
char DIREC[256]="tmp";

int get_file_size(char* filename)
{
    struct stat statbuf;
    stat(filename,&statbuf);
    int size=statbuf.st_size;

    return size;
}

int get_data_socket(int* fd,int listenSocket,char *clientIp,int clientPort){
    struct sockaddr_in inaddr;
    socklen_t socklen = sizeof(inaddr);
    if(listenSocket != 0){                                                  //服务器监听
        *fd = accept(listenSocket,(struct sockaddr *)&inaddr, &socklen);
        if (*fd == -1) return -1;
    }
    else{
        struct sockaddr_in addr;
        if ((*fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
            printf("Error socket(): %s(%d)\n", strerror(errno), errno);
            return -1;
        }

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons((uint16_t)clientPort);
        if (inet_pton(AF_INET, clientIp, &addr.sin_addr) <= 0) {
            printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
            return -1;
        }

        if (connect(*fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            printf("Error connect(): %s(%d)\n", strerror(errno), errno);
            return -1;
        }
    }
    return 0;
}

int get_local_ip(const char *eth_inf, char *ip)
{
    int sd;
    struct sockaddr_in sin;
    struct ifreq ifr;

    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (-1 == sd)
    {
        printf("socket error: %s\n", strerror(errno));
        return -1;
    }

    strncpy(ifr.ifr_name, eth_inf, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;

    // if error: No such device
    if (ioctl(sd, SIOCGIFADDR, &ifr) < 0)
    {
        printf("ioctl error: %s\n", strerror(errno));
        close(sd);
        return -1;
    }

    memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
    snprintf(ip, IP_SIZE, "%s", inet_ntoa(sin.sin_addr));

    close(sd);
    return 0;
}

int getAbsolutePath(char* ret,char*curPath,char* path){
    chdir(curPath);
    DIR *dir;
    if(path != NULL) {
        if (!chdir(path)) {
            getcwd(ret, (size_t) BUFLIMIT);
            chdir(curPath);
            return 0;
        }
        else if ((dir=opendir(path)) == NULL)
        {
            strcpy(ret,path);
            return 1;
        }
    }
    strcpy(ret,curPath);
    return 0;
}





void toUpper(char* p){
    for(int i = 0;i<strlen(p);i++){
        p[i] = (char) toupper(p[i]);
    }
}

int remove_dir(const char *dir) {
    char cur_dir[] = ".";
    char up_dir[] = "..";
    char dir_name[1024]={0};
    DIR *dirp;
    struct dirent *dp;
    struct stat dir_stat;

    // 参数传递进来的目录不存在，直接返回
    if ( 0 != access(dir, F_OK) ) {
        return 0;
    }

    // 获取目录属性失败，返回错误
    if ( 0 > stat(dir, &dir_stat) ) {
        perror("get directory stat error");
        return -1;
    }

    if ( S_ISREG(dir_stat.st_mode) ) {  // 普通文件直接删除
        remove(dir);
    } else if ( S_ISDIR(dir_stat.st_mode) ) {   // 目录文件，递归删除目录中内容
        dirp = opendir(dir);
        while ( (dp=readdir(dirp)) != NULL ) {
            // 忽略 . 和 ..
            if ( (0 == strcmp(cur_dir, dp->d_name)) || (0 == strcmp(up_dir, dp->d_name)) ) {
                continue;
            }
            memset(dir_name,0,256);
            snprintf(dir_name,sizeof(dir_name), "%s/%s", dir, dp->d_name);
            remove_dir(dir_name);   // 递归调用
        }
        closedir(dirp);

        rmdir(dir);     // 删除空目录
    } else {
        perror("unknow file type!");
    }

    return 0;
}

void writeRet(char* ret,char* code,char *p,char* c){
    memset(ret,0,strlen(ret));
    strcpy(ret,code);
    strcat(ret,c);
    strcat(ret,p);
    strcat(ret,"\r\n");
}

int GetCmdFromClient(int connfd,char* cmd,int MAXSIZE){
    int p = 0;
    memset(cmd, 0, (size_t) MAXSIZE);
    while (1) {
        int n = (int) read(connfd, cmd + p, (size_t) (MAXSIZE - p));
        if (n < 0) {
            printf("Error read(): %s(%d)\n", strerror(errno), errno);
            close(connfd);
            continue;
        } else if (n == 0) {
            break;
        } else {
            p += n;
            if (cmd[p - 1] == '\n' || cmd[p - 2] == '\r') {
                break;
            }
        }
    }
    if(p>2) {
        cmd[p - 2] = '\0';
        return p - 2;
    }
    return p;
}

int WriteToClient(int connfd,char* ret,int len){

    int p = 0;
    if(len<=0){
        return 0;
    }
    while (p < len) {
        int n = (int) write(connfd, ret + p, (size_t) (len - p));
        if (n < 0) {
//            printf("Error write(): %s(%d)\n", strerror(errno), errno);
            pthread_exit(NULL);
            return 1;
        } else {
            p += n;
        }
    }
    return 0;
}


void* download(void* load){
    struct loadMsg* loadMsg1 = (struct loadMsg*) load;
    char databuf[8192] = {0};
    int len = 0;
    int fd = loadMsg1->fd;
    int datafd = loadMsg1->datafd;
    struct Client* client = loadMsg1->client;
    while ((len = (int) read(fd, databuf, sizeof(databuf))) > 0) {
        int retu = send(datafd, databuf, len, 0);
        if (retu < 0) {
            close(fd);
            writeRet(client->ret, "426", "network wrong", " ");
            WriteToClient(client->connfd, client->ret, (int) strlen(client->ret));
            if (client->listenfd > 0)
                close(client->listenfd);
            client->listenfd = 0;
            close(datafd);
            client->data_socket = 0;
            client->in_loading = 0;
            return NULL;
        }
    }
    close(fd);
    memset(client->ret,0,strlen(client->ret));
    writeRet(client->ret,"226","Ransfer Complete"," ");
    WriteToClient(client->connfd, client->ret, (int) strlen(client->ret));
    if(client->listenfd>0)
        close(client->listenfd);
    client->listenfd = 0;
    close(datafd);
    client->data_socket = 0;
    client->offset = 0;
    client->in_loading = 0;
    return NULL;
}





char* scrach(char *s){
    int len = 0;
    for(int i = 0;s[i];i++){
        if(s[i]==' '||s[i]=='\n') {
            break;
        }
        else
            len++;
    }
    len++;
    if(len<=1){
        return NULL;
    }

    char* cmd = (char*)malloc(sizeof(char)*len);
    for (int i = 0;i<len-1;i++) {
        cmd[i] = s[i];
    }
    cmd[len-1]='\0';
    return cmd;
}

int CheckNumString(char* p){
    int len = (int) strlen(p);
    for(int i = 0;i<len;i++){
        if(p[i]<'0'||p[i]>'9'){
            return 0;
        }
    }
    return 1;
}

int CharToInt(char *p){
    int len = (int) strlen(p);
    int sum = 0;
    for(int i = 0;i<len;i++){
        sum *=10;
        sum += (p[i]-'0');
    }
    return sum;
}

//int USER_Handler(char* org,char* ret,int* loginStatus)
int USER_Handler(struct Client* client)
{

    strtok(client->sentence,split);
    char* p = strtok(NULL,split);
    if(p == NULL){
        client->login = 0;
        writeRet(client->ret,"530","We support anonymous login only"," ");
    }
    else {
        if (!strcmp(p, "anonymous")) {
            client->login = 1;
            writeRet(client->ret,"331","Guest login ok, send your complete e-mail address as password."," ");
        } else {
            client->login = 0;
            writeRet(client->ret,"530","We support anonymous login only"," ");
        }
    }
    return 0;
}

int PASS_Handler(struct Client* client){
    if (client->login == 0){
        writeRet(client->ret,"503","Please input User first"," ");
    }
    else if(client->login == 1) {
        regex_t reg;
        strtok(client->sentence,split);
        int err;
        regmatch_t pmatch[10];
        char* p = strtok(NULL,split);
        if(p == NULL){
            writeRet(client->ret,"501","Please input a email Address first"," ");
        }
        char *pattern = ".*@.*";
        regcomp(&reg,pattern,REG_EXTENDED);
        err = regexec(&reg,p,10,pmatch,0);
        if(err == REG_NOMATCH){
            writeRet(client->ret,"501","Please input a email Address first"," ");
        }
        else{
            writeRet(client->ret,"230","Welcome"," ");
            client->login = 2;
        }
    }
    return 0;
}





int SYST_Handler(struct Client* client){
    if(client->login < 2){
        writeRet(client->ret,"503","Please input User first"," ");

    }
    else {
        writeRet(client->ret,"215","UNIX Type: L8"," ");
    }
    return 0;
}


int TYPE_Handler(struct Client* client) {
    if(client->login < 2){
        writeRet(client->ret,"503","Please input User first"," ");

    }
    else {
        char* p;
        strtok(client->sentence,split);
        p = strtok(NULL,split);
        if(!strcmp(p,"I")){
            writeRet(client->ret,"200","Type set to I."," ");
        }
        else{
            writeRet(client->ret,"504","Type set failed."," ");
        }
    }
    return 0;
}

int PWD_Handler(struct Client* client) {
    if(client->login < 2){
        writeRet(client->ret,"503","Please input User first"," ");
    }
    else {
        char sr[8192];
        strcpy(sr,"\"");
        strcat(sr,client->cwd);
        strcat(sr,"\"");
        writeRet(client->ret,"257",sr," ");
    }
    return 0;
}

int CWD_Handler(struct Client* client) {
    if(client->login < 2){
        writeRet(client->ret,"503","Please input User first"," ");
    }
    else {
        char* p;
        strtok(client->sentence,split);
        p = strtok(NULL,split);
        if(p == NULL){
            writeRet(client->ret,"501","please input a folder name"," ");
        }
        else {
            //char newPath[8192] = {0};
            //getAboslutePath(newPath,cwd,p);
            chdir(client->cwd);
            if(!access(p,0)){
                if(chdir(p) == -1)
                {
                    writeRet(client->ret,"550","chdir failed"," ");
                }
                else {
                    getcwd(client->cwd, (size_t) BUFLIMIT);
                    writeRet(client->ret, "250", "Okay"," ");
                }
            }
            else{
                writeRet(client->ret,"550","no such file"," ");
            }

        }
    }
    return 0;
}

int MKD_Handler(struct Client* client) {
    if(client->login < 2){
        writeRet(client->ret,"503","Please input User first"," ");
    }
    else {
        char* p;
        strtok(client->sentence,split);
        p = strtok(NULL,split);
        if(p == NULL){
            writeRet(client->ret,"501","please enter a folder name"," ");
        }
        else {
            char newDir[8192]= {0};
//            getAboslutePath(newDir,cwd,p);
            if(mkdir(p,0755) == -1) {
                writeRet(client->ret,"550","failed to create a folder."," ");
            }
            else {
                chdir(p);
                getcwd(newDir,BUFLIMIT);
                chdir(client->cwd);
                char aa[8192]={0};
                strcpy(aa,"\"");
                strcat(aa,newDir);
                strcat(aa,"\"");
                writeRet(client->ret, "250", aa," ");
            }
        }
    }
    return 0;
}

int RMD_Handler(struct Client* client) {
    if(client->login < 2){
        writeRet(client->ret,"503","Please input User fisrt"," ");
    }
    else {
        chdir(client->cwd);
        char* p;
        strtok(client->sentence,split);
        p = strtok(NULL,split);
        if(p == NULL){
            writeRet(client->ret,"501","please enter a folder name"," ");
        }
        if(remove_dir(p)<0){
            writeRet(client->ret,"550","Failed to remove"," ");
        }
        else{
            writeRet(client->ret,"250","Remove Successfully"," ");
        }
    }
    return 0;
}


int _list(char* basePath,char* ret,int datafd) {
    DIR *dir;
    struct dirent *ptr;
    char edit[8192]={0};
    char base[1000]={0};
    char Info[1000]={0};
    if ((dir=opendir(basePath)) == NULL)
    {
        struct stat stbuf;
        int fd;

        fd = open(basePath, O_RDONLY);
        if (fd == -1) {
            return -1;
        }
        if ((fstat(fd, &stbuf) != 0) || (!S_ISREG(stbuf.st_mode))) {
            return -1;
        }
        char si[100]={0};
        sprintf(si, "%d", (int) stbuf.st_size);
        strcpy(Info,"path:");
        strcat(Info,basePath);
        strcat(Info,"      size:");
        strcat(Info,si);
//        writeRet(ret,"200",Info," ");
        write(datafd,Info,strlen(Info));
        return 0;
    }

    while ((ptr=readdir(dir)) != NULL)
    {
        if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0)    ///current dir OR parrent dir
            continue;
        else{
            if(ptr->d_type == 8)
                strcpy(Info,"File     :");
            else if(ptr->d_type == 10)
                strcpy(Info,"Link File:");
            else if(ptr->d_type == 4)
                strcpy(Info,"Dir      :");
            strcat(Info, ptr->d_name);
            strcat(Info,"\n");
            write(datafd,Info,strlen(Info));
        }
        strcat(edit,base);
    }
    strcat(edit,base);
    closedir(dir);
    strcpy(ret,edit);
    return 0;
}



int LIST_Handler(struct Client* client) {
    if(client->login < 2){
        writeRet(client->ret,"503","Please input User fisrt"," ");
    }
    else {
        char *p;
        strtok(client->sentence, split);
        p = strtok(NULL, split);
        char abPath[8192] = {0};
        if (getAbsolutePath(abPath, client->cwd, p) < 0) {
            writeRet(client->ret, "550", "failed to acess the path", " ");
        }
        else {
            int fd;
            fd = open(abPath, O_RDONLY);
            char databuf[8192] = {0};
            if (fd == -1) {
                writeRet(client->ret, "451", "failed to open the file", " ");
            }
            else if (get_data_socket(&fd, client->listenfd, client->ClientIP, client->ClientPort) < 0) {
                writeRet(client->ret, "425", "failed to establish a data socket", " ");
            }
            else {
                writeRet(client->ret, "150", " Opening BINARY mode data connection for.", " ");
                WriteToClient(client->connfd, client->ret, strlen(client->ret));
                if (_list(abPath, client->ret,fd) < 0) {
                    writeRet(client->ret, "550", "failed to LIST", " ");
                }
                else{
                    sleep(0.001);
                    writeRet(client->ret,"226","transfer complete"," ");
                }
                if(client->listenfd>0)
                    close(client->listenfd);
                client->listenfd = 0;
                close(fd);
            }
        }
    }
    return 0;
}



int RNFR_Handler(struct Client* client) {

    char *p;
    strtok(client->sentence, split);
    p = strtok(NULL, split);
//    char newPath[8192]={0};
//    getAboslutePath(newPath,cwd,p);

    if(!access(p,F_OK)) {
        strcpy(client->fileSelected,p);
        writeRet(client->ret,"350","file exists"," ");
    }
    else{
        writeRet(client->ret,"550","file is unusable"," ");
    }
    return 0;
}

int RNTO_Handler(struct Client* client) {
    char sysOrder[8192]={0};
    char *p;
    strtok(client->sentence, split);
    p = strtok(NULL, split);
    {
        if (!strcmp(client->lastCMD, "RNFR")) {
            if (!access(client->fileSelected, F_OK)) {
                strcpy(sysOrder, "mv");
                strcat(sysOrder, " ");
                strcat(sysOrder, client->fileSelected);
                strcat(sysOrder, " ");
                strcat(sysOrder, p);
                int status = system(sysOrder);
                if (WIFEXITED(status)) {
                    if (0 == WEXITSTATUS(status)) {
                        writeRet(client->ret,"250","renamed successfully"," ");
                    }
                    else {
                        writeRet(client->ret,"550","renamed failed"," ");
                    }
                }
                else {
                    writeRet(client->ret,"550","renamed failed"," ");
                }

            } else {
                writeRet(client->ret, "550", "src unusable", " ");
            }
        }
        else{
            writeRet(client->ret, "550", "RNFR First", " ");
        }
    }
}


//not comlete
int PORT_Handler(struct Client* client){
    if(client->login != 2){
        writeRet(client->ret,"503","Please input User first"," ");

    }
    else {
        char newIp[IPLENGTH]={0};
        int newPort = 0;
        char *p;
        strtok(client->sentence, split);
        memset(client->ClientIP,0,strlen(client->ClientIP));
        for (int i = 0; i < 6; i++) {
            p = strtok(NULL, split);
            if (p == NULL) {
                strcpy(client->ret, "425 IP Wrong");
                return -1;
            }
            if (CheckNumString(p)) {
                if(i<4) {
                    strcat(newIp, p);
                    if (i != 3)
                        strcat(newIp, ".");
                }
                else{
                    if(i == 4){
                        newPort = CharToInt(p)*256;
                    }
                    else{
                        newPort += CharToInt(p);
                    }
                }
            }
        }

        if(client->data_socket != 0) {
            close(client->data_socket);
            client->data_socket = 0;
        }
        if(client->listenfd != 0){
            close(client->listenfd);
            client->listenfd = 0;
        }

        strcpy(client->ClientIP,newIp);
        client->ClientPort = newPort;
        writeRet(client->ret,"200","Port Success!"," ");
    }
    return 0;
}

//not complete
int PASV_Handler(struct Client* client){
    //判断登录
    if(client->login < 2){
        writeRet(client->ret,"503","Please input User first"," ");

    }
    else{

        char ip[IP_SIZE];
        char msg[8192];
        int port;

        //获取IP
        if(get_local_ip("lo",ip)<0){
            writeRet(client->ret,"550","failed to get server ip"," ");
        }
        else{
            //处理IP
            for(int i = 0;i<strlen(ip);i++){
                if(ip[i] == '.')
                    ip[i]=',';
            }
            strcpy(msg,ip);
//            获取端口
            port = (int) (random() % (65536 - 20000) + 20000);
            char portStr[100];
            sprintf(portStr,",%d,%d",port/256,port%256);
            strcat(msg,portStr);


            if(client->data_socket != 0) {
                close(client->data_socket);
                client->data_socket = 0;
            }
            if(client->listenfd != 0){
                close(client->listenfd);
                client->listenfd = 0;
            }
//            新建监听Socket
            if ((client->listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
                writeRet(client->ret,"550","failed to getSocket"," ");
                client->listenfd = 0;
                return -1;
            }

            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = htonl(INADDR_ANY);

            if (bind(client->listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
                writeRet(client->ret,"550","failed to bind"," ");
                client->listenfd = 0;
                return -1;
            }
//            开始监听
            if (listen(client->listenfd, 10) == -1) {
                writeRet(client->ret,"550","failed to listen"," ");
                client->listenfd = 0;
                return -1;
            }
            writeRet(client->ret,"227",msg," ");
        }

    }
    return 0;
}


int RETR_Handler(struct Client* client){
    if(client->login < 2){
        writeRet(client->ret,"503","Please input User first"," ");

    }
    else{
        strtok(client->sentence, split);
        char* p = strtok(NULL,split);
        char abPath[256];
        chdir(client->cwd);
        int fd;
        if(getAbsolutePath(abPath,client->cwd,p) != 1){
            writeRet(client->ret,"451","failed to get the file"," ");
        }
        else{
            int datafd;
            fd = open(abPath, O_RDONLY);
            char databuf[8192] = {0};
            if (fd == -1) {
                writeRet(client->ret, "451","failed to open the file"," ");
            }
            else if(get_data_socket(&datafd,client->listenfd,client->ClientIP,client->ClientPort)<0){
                writeRet(client->ret,"425","failed to establish a data socket"," ");
            }

            else{
                int filesize = get_file_size(abPath);
                char ret_size[8191];
                sprintf(ret_size," Opening BINARY mode data connection for %s (%d bytes).",p,filesize);
                writeRet(client->ret,"150",ret_size," ");
                sleep(0.0001);
                int len = 0;
                int offset = client->offset;

                lseek(fd, offset, SEEK_SET);
                struct loadMsg loadMsg1;
                loadMsg1.fd = fd;
                loadMsg1.datafd = datafd;
                loadMsg1.client = client;
                pthread_t  downloadthread;
                client->in_loading = 1;
                int ret = pthread_create(&downloadthread, NULL, download, (void*)&loadMsg1 );


//                while ((len = (int) read(fd, databuf, sizeof(databuf))) > 0) {
//                    int retu = send(datafd, databuf, len, 0);
//                    if (retu < 0) {
//                        close(fd);
//                        writeRet(client->ret, "426", "network wrong", " ");
//                        if (client->listenfd > 0)
//                            close(client->listenfd);
//                        client->listenfd = 0;
//                        close(datafd);
//                        client->data_socket = 0;
//                        return 1;
//                    }
//                }
//                close(fd);
//                writeRet(client->ret,"226","Ransfer Complete"," ");
//                if(client->listenfd>0)
//                    close(client->listenfd);
//                client->listenfd = 0;
//                close(datafd);
//                client->data_socket = 0;
//                client->offset = 0;
            }
        }
    }
    return 0;
}

void* upload(void* load){
    struct loadMsg* loadMsg1 = (struct loadMsg*) load;
    char databuf[8192] = {0};
    int len = 0;
    int fd = loadMsg1->fd;
    int datafd = loadMsg1->datafd;
    struct Client* client = loadMsg1->client;

    while ((len = (int) read(datafd, databuf, sizeof(databuf))) > 0) {
        int retu = (int) write(fd, databuf, len);
        if (retu < 0) {
            close(fd);
            writeRet(client->ret, "426", "network wrong", " ");
            WriteToClient(client->connfd, client->ret, (int) strlen(client->ret));
            if (client->listenfd > 0)
                close(client->listenfd);
            client->listenfd = 0;
            close(datafd);
            client->data_socket = 0;
            client->in_loading = 0;
            return NULL;
        }
    }
    close(fd);
    writeRet(client->ret,"226","Ransfer Complete"," ");
    WriteToClient(client->connfd, client->ret, (int) strlen(client->ret));
    if(client->listenfd>0)
        close(client->listenfd);
    client->listenfd = 0;
    close(datafd);
    client->data_socket = 0;
    client->in_loading = 0;
    return NULL;
}

int STOR_Handler(struct Client* client){
    if(client->login< 2){
        writeRet(client->ret,"503","Please input User first"," ");
    }
    else {
        strtok(client->sentence, split);
        char* p = strtok(NULL,split);
        if(p == NULL){
            writeRet(client->ret,"450","no file input"," ");
        }
        else{
            char filename[8192]={0};
            int index = 0;
            for(int i = 0;i<strlen(p);i++){
                if(p[i] != '/')
                    filename[index++] = p[i];
                else{
                    index = 0;
                    memset(filename,0,8192);
                }
            }
            chdir(client->cwd);
            int datafd;
            fopen(filename, "w+");
            int fd = open(filename,O_WRONLY);
            char databuf[8192] = {0};
            if (fd == -1) {
                close(datafd);
                writeRet(client->ret, "451","failed to open the file"," ");
            }
            else if(get_data_socket(&datafd,client->listenfd,client->ClientIP,client->ClientPort)<0){
                writeRet(client->ret,"425","failed to establish a data socket"," ");
            }
            else {
                writeRet(client->ret, "150", " Opening BINARY mode data connection for.", " ");

                sleep(0.0001);
                int len = 0;
                int offset = 0;
                lseek(fd, offset, SEEK_SET);

                struct loadMsg loadMsg1;
                loadMsg1.fd = fd;
                loadMsg1.datafd = datafd;
                loadMsg1.client = client;

                pthread_t uploadthread;
                client->in_loading = 1;
                int ret = pthread_create(&uploadthread, NULL, upload, (void*)&loadMsg1 );


//                while ((len = (int) read(datafd, databuf, sizeof(databuf))) > 0) {
//                    int retu = write(fd, databuf, len);
//                    if (retu < 0) {
//                        close(fd);
//                        writeRet(client->ret, "426", "network wrong", " ");
//                        if (client->listenfd > 0)
//                            close(client->listenfd);
//                        client->listenfd = 0;
//                        close(datafd);
//                        client->data_socket = 0;
//                        return 1;
//                    }
//                }
//                close(fd);
//                writeRet(client->ret,"226","Ransfer Complete"," ");
//                if(client->listenfd>0)
//                    close(client->listenfd);
//                client->listenfd = 0;
//                close(datafd);
//                client->data_socket = 0;

            }

        }
    }
    return 0;
}


int APPE_Handler(struct Client* client){
    if(client->login< 2){
        writeRet(client->ret,"503","Please input User first"," ");
    }
    else {
        strtok(client->sentence, split);
        char* p = strtok(NULL,split);
        if(p == NULL){
            writeRet(client->ret,"450","no file input"," ");
        }
        else{
            char filename[8192]={0};
            int index = 0;
            for(int i = 0;i<strlen(p);i++){
                if(p[i] != '/')
                    filename[index++] = p[i];
                else{
                    index = 0;
                    memset(filename,0,8192);
                }
            }
            chdir(client->cwd);
            int datafd;
//            fopen(filename, "w+");
            int fd = open(filename,O_WRONLY|O_APPEND);
            char databuf[8192] = {0};
            if (fd == -1) {
                close(datafd);
                writeRet(client->ret, "451","failed to open the file"," ");
            }
            else if(get_data_socket(&datafd,client->listenfd,client->ClientIP,client->ClientPort)<0){
                writeRet(client->ret,"425","failed to establish a data socket"," ");
            }
            else {
                writeRet(client->ret, "150", " Opening BINARY mode data connection for.", " ");

                sleep(0.0001);
                int len = 0;

                struct loadMsg loadMsg1;
                loadMsg1.fd = fd;
                loadMsg1.datafd = datafd;
                loadMsg1.client = client;
                pthread_t uploadthread;
                client->in_loading = 1;
                int ret = pthread_create(&uploadthread, NULL, upload, (void*)&loadMsg1 );

//                while ((len = (int) read(datafd, databuf, sizeof(databuf))) > 0) {
//                    int retu = write(fd, databuf, len);
//                    if (retu < 0) {
//                        close(fd);
//                        writeRet(client->ret, "426", "network wrong", " ");
//                        if (client->listenfd > 0)
//                            close(client->listenfd);
//                        client->listenfd = 0;
//                        close(datafd);
//                        client->data_socket = 0;
//                        return 1;
//                    }
//                }
//                close(fd);
//                writeRet(client->ret,"226","Ransfer Complete"," ");
//                if(client->listenfd>0)
//                    close(client->listenfd);
//                client->listenfd = 0;
//                close(datafd);
//                client->data_socket = 0;

            }

        }
    }
    return 0;
}


int QUIT_Handler(struct Client* client){
    if(client->login > 0)
        close(client->listenfd);
    if(client->data_socket>0)
        close(client->data_socket);
    writeRet(client->ret,"221","Bye"," ");
    WriteToClient(client->connfd, client->ret, (int) strlen(client->ret));
    close(client->connfd);
    pthread_exit(NULL);
}

int REST_Handler(struct Client* client){
    if(client->login< 2){
        writeRet(client->ret,"503","Please input User first"," ");
    }
    else {
        strtok(client->sentence, split);
        char* p = strtok(NULL,split);
        if(p == NULL){
            writeRet(client->ret,"550","no file input"," ");
        }
        else {
            client->offset = atoi(p);
            writeRet(client->ret,"350","set start point"," ");

        }
    }
}






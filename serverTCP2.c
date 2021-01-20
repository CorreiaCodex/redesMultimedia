#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>

#include <dirent.h>
#include "sendStream.h"

#define PORT 6001
#define BUFFERSIZE 1024
#define BACKLOG 5
#define NUM_THREADS 1000
#define HEADERSIZE 7

typedef struct {
    uint8_t version;
    uint16_t size;
    uint32_t counter;
} MSGHeader;

struct myargs{
    char *file;
    char * ip;
    int cport;
    int connfd;
};


static volatile int keepRunning = 1;

void intHandler(int sig) {
    keepRunning = 0;
}

void *sendClip(void *u){
    char *clip;
    int cports;
    char *addr;
    int connfd,n;
    struct myargs *my_data;
    char cmd[BUFFERSIZE];

    my_data = (struct myargs *) u;
    clip = my_data -> file;
    addr = my_data -> ip;
    cports = my_data -> cport; 
    connfd = my_data -> connfd;

    printf("file thread_: %s\n",my_data->file );
   // printf("cports_:%d\n",cport );
    //printf("putas e \n");
    sendStream(clip,addr,cports);
    printf("TOUUUUUUUUUUU\n");
    
    /*printf("commando: ");
    n = read(connfd, (char *)cmd, strlen(cmd));
    if(n<0){
        printf("commando do client: non\n");
    }else{
        printf("client command: %s\n",cmd);
    }*/
    
}

void *clientHandler(void *fd)
{
    char inmsg[BUFFERSIZE],outmsg[BUFFERSIZE+16],allmsg[BUFFERSIZE+16+HEADERSIZE];
    char msg[BUFFERSIZE];
    int n,cport,connfd=*(int*)fd,lenaddr;
    struct sockaddr_in cliaddr;
    MSGHeader header;

    lenaddr = sizeof(cliaddr);
    getpeername(connfd, (struct sockaddr *)&cliaddr, &lenaddr);
    cport=ntohs(cliaddr.sin_port);   // To ensure correct endianness

    struct myargs args;

    while(1){

        bzero(&header, HEADERSIZE);
        bzero(allmsg, BUFFERSIZE+16+HEADERSIZE);
        bzero(outmsg,BUFFERSIZE);

        bzero(msg,BUFFERSIZE);
/////////////////////////////////////////////////////////////
        // LISTA TODOS OS FICHEIROS .TS AO CLIENT
        DIR *dir;
        struct dirent *lsdir;
        dir = opendir("/home/correia/Desktop/RM/codigoRMv1.0");
        int i = 0;
        char fileName[20];
        char list[BUFFERSIZE]="";

        for(;;){
          lsdir = readdir(dir);
          if(lsdir == NULL){
            printf("Nothing more...\n");
            break;
          }
          if (strcmp(lsdir -> d_name,".")){
            if (strcmp(lsdir -> d_name,"..")){
              if(strstr(lsdir -> d_name, ".ts")){
                strcpy(fileName,lsdir->d_name);
                printf("%d -> ",i);
                printf("%s\n", fileName);
                strcat(strcat(list,fileName),"\n");
                i++;
              }
            }
          }
        }
        closedir(dir); 

        header.size=strlen(list);
        memcpy(allmsg,&header,HEADERSIZE);
        memcpy(&allmsg[HEADERSIZE],list,header.size);
        
        /////send list to client
        if(write(connfd, allmsg,HEADERSIZE+header.size)<0){
            printf("Client (%s:%d) OFF\n",inet_ntoa(cliaddr.sin_addr),cport);
            bzero(allmsg,BUFFERSIZE);
            close(connfd);
            return NULL;
        }else{       
            printf("ECHO message sent to (%s:%d).\n",inet_ntoa(cliaddr.sin_addr),cport);
            bzero(allmsg,BUFFERSIZE);
        }
//////////////////////////////////////////////////////////////////

        ////// AGUARDA PELA ESCOLHA DO CLIENT
        bzero(inmsg, BUFFERSIZE);
       // header.size =strlen(inmsg);
        n = read(connfd, (char *)inmsg, header.size);
        if(n<0)
        {
            printf("Client (%s:%d) gone\n",inet_ntoa(cliaddr.sin_addr),cport);
            close(connfd);
            return NULL;
        }
        
        char file[BUFFERSIZE];
        bzero(file,BUFFERSIZE);
        printf("Client (%s:%d): %s\n",inet_ntoa(cliaddr.sin_addr),cport,inmsg);
        
        pthread_t tid;
        for(int x = 0;x<strlen(inmsg)-1;x++){   ///apenas server para remover o caracter
            file[x] = inmsg[x];                 /// ENTER
        }
        if(strcmp(file,"exit")==0){
            printf("BYE\n");
            close(connfd);
            pthread_cancel(tid);                  ///removido 19/01->23h34
        }
        if(strcmp(file,"s")==0){
            pthread_cancel(tid);
        }//////o proximo else pode ser removido se nao funcionar
        
        strcat(file,".ts");
/////////////////////////////////////////////////////////////
        // VERIFICA SE O CLIENT ESCOLHEU UM VIDEO CORRETAMENTE

        FILE *dataFile;                 //checks if file exists befor playing it
        dataFile=fopen(file,"rb"); 
        char info[] = "SERVER:404->File not found";   

        if(dataFile==NULL){
            printf("Server didn't find this file!\n");
            n = write(connfd,info,strlen(info));   //in non exitence case tell client
            //return 2;
        }else{
            printf("FILE IN SERVER\n");
            n = write(connfd,"1",strlen("1"));
        }

        printf("121:file->>>: %s\n ",file);
    //////////////////////////////////////////////////////////
            //ARGUMENTOS A PASSAR A THREAD PLAY
        args.file = file;
        char ip_addr[] = "127.0.0.1";
        args.ip = ip_addr;
        args.cport = cport;
        args.connfd = connfd;

        printf("filename----:%s\n",args.file);
        pthread_create(&tid,NULL,sendClip,(void *)&args);
        //pthread_detach(tid);
        
        printf("129:oi cao de merda\n");

        //bzero(&header, HEADERSIZE);
        //printf("Packet %u: version %u, payload size %u\n",header.counter,header.version,header.size);
        
        //////////////////////////////////////////////////////////////
    }
    close(connfd);
}


int main() {
    int sockfd,connfd;
    int lenaddr;
    struct sockaddr_in servaddr, cliaddr;
    unsigned short cport;
    fd_set rset;
    pthread_t  threadid;

    signal (SIGQUIT, intHandler);
    signal (SIGINT, intHandler);

    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // Filling server information
    servaddr.sin_family    = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    // Bind the socket with the server address
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,
            sizeof(servaddr)) < 0 )
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }


    // Now server is ready to listen and verification
    if ((listen(sockfd, BACKLOG)) != 0) {
        printf("Listen failed...\n");
        exit(0);
    }
    else
        printf("Server listening 1..\n");

    FD_ZERO(&rset);

    lenaddr = sizeof(cliaddr);
    while(keepRunning){
        FD_SET(sockfd, &rset);

        // select the ready descriptor
        select(sockfd+1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(sockfd, &rset) && keepRunning){
            // Accept the data packet from client and verification
            connfd = accept(sockfd, (struct sockaddr *)&cliaddr, &lenaddr);
            if (connfd < 0) {
                printf("server acccept failed...\n");
                exit(0);
            }
            else
                printf("server acccept the client...\n");
        }

        cport=ntohs(cliaddr.sin_port);   // To ensure correct endianness
        printf("Client (%s:%d) arrived\n",inet_ntoa(cliaddr.sin_addr),cport);

        //


        if (pthread_create(&threadid, NULL, clientHandler, &connfd)<0) {
            printf("Error:unable to create thread\n");
            return -1;
        }
        pthread_detach(threadid);
    }

    printf("Server stopping...\n");
    close(sockfd);
    printf("Server stop!\n");

    return 0;
}

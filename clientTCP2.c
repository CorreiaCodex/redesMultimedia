#include <stdio.h> 
#include <stdlib.h> 
#include <signal.h>
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <pthread.h>

  
#define PORT 6001
#define BUFFERSIZE 1024
#define HEADERSIZE 7



typedef struct {
    uint8_t version;
    uint16_t size;
    uint32_t counter;
} MSGHeader; 

static volatile int keepRunning = 1;

void intHandler(int sig) {
    keepRunning = 0;
}

void *cmdCommand(void *u){

    char command[256];
    int cport = *((int *)u);
    sprintf(command, "ffplay -hide_banner -loglevel quiet rtp://127.0.0.1:%d",cport);
    system(command);
}

int main() {

    char ip_addr[16]="127.0.0.1"; 
    char msg[BUFFERSIZE],allmsg[BUFFERSIZE+HEADERSIZE],outmsg[BUFFERSIZE];; 
    char cmd[BUFFERSIZE];
    struct sockaddr_in servaddr,myaddr; 
    int n, len,lenaddr,nmsg=0,cport;
    int sockfd, myport; 

    fd_set rset;
    MSGHeader header;
    
    signal (SIGQUIT, intHandler);
    signal (SIGINT, intHandler);


    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
  
    memset(&servaddr, 0, sizeof(servaddr)); 
      
    // Filling server information 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(PORT); 
    servaddr.sin_addr.s_addr = inet_addr(ip_addr); 
    lenaddr=sizeof(myaddr);
    
    // connect the client socket to server socket 
    if (connect(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) { 
        printf("Connection with the server failed...\n"); 
        exit(0); 
    } 
    else
    {
        getsockname(sockfd, (struct sockaddr *)&myaddr, &lenaddr);
        cport=ntohs(myaddr.sin_port);
        printf("Connected to the server using local port %d\n",cport); 
    }
    /////////////////////////////
    while(keepRunning){
        //////////////////////////////////////////////////
        //MAL SE CONNECT AO SERVER FICA A ESPERA DA LISTA 
        FD_SET(sockfd, &rset);
        // select the ready descriptor 
        select(sockfd+1, &rset, NULL, NULL, NULL);
        if (FD_ISSET(sockfd, &rset) && keepRunning){
            n = read(sockfd, &header, HEADERSIZE); /////SE NAO DER DESCOMENTAR
            bzero(msg,BUFFERSIZE);
            n = read(sockfd, msg, header.size);
            printf("->Type 'exit' to quit<-\n");
            printf("Choose clip: \n");
            printf("%s", msg); 
        }

        bzero(msg,BUFFERSIZE);
        strcpy(allmsg,"");
        strcpy(outmsg,"");
//////////////////////////////////////////////////////////
        /// ESCOLHE UM VIDEO E PEDE-O AO SERVER
        pthread_t id;
        
        bzero(cmd,BUFFERSIZE);
        printf("Message: ");
        fgets(outmsg,BUFFERSIZE,stdin);

        for(int x = 0;x<strlen(outmsg)-1;x++){
            cmd[x] = outmsg[x];
        }
        if(strcmp(cmd,"exit")==0){
            keepRunning = 0;    
        }
        if(strcmp(cmd,"s")==0){
            printf("Clips stopped, you can choose another one or type 'exit' to quit\n");
            pthread_cancel(id);
        }
       
        header.version=2;
        header.counter = nmsg++;
        header.size = strlen(outmsg);

        memcpy(allmsg, &header, HEADERSIZE); 
        memcpy(&allmsg[HEADERSIZE], outmsg, header.size); 
      
        
        if(write(sockfd,outmsg,HEADERSIZE+header.size)<0){
            printf("%s", "Server off");
        }else{
            printf("-----> Message sent to server.\n\n");
        }
      
        ///////////////////////////////////////////////////
          //waits for file existance from server
        // CASO NAO EXISTA OU TENHA ECRITO MAL O NOME- REPETE ATE ESCOLHER CORRETAMENTE
        bzero(msg,BUFFERSIZE);
        n = read(sockfd,msg,BUFFERSIZE);
        
        if(strcmp(msg,"1")==0){
            printf("WAIT A FEW SECONDS, IT WILL PLAY IN A BLINK OF AN EYE\n");
            printf("TYPE 's' TO STOP CLIP\n");            
            pthread_create(&id,NULL,cmdCommand,(void *) &cport);
            pthread_detach(id);            
        }
        else{
            printf("WARNING SENDED BY SERVER:\n%s\n\n",msg );
            printf("->CLIENT WARNING:\n-->TYPE THE CLIP NAME CORRECTLY!!!\n-->DOESN'T NEED TERMINATION '.ts'\n");
            //pthread_detach(id);
        }

        
    }
    
    printf("\nClient stopping...\n");
    close(sockfd);
    printf("Client stop!\n");
    return 0; 
} 

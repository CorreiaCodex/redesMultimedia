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
#include "RTPHeader.h"

#define TSSIZE 188
#define MAXTS 7
//#define PORT 6001       //////5001
#define MAXSIZE 1500 
#define HEADERSIZE 7   ////////10 
#define RTPHEADERSIZE 12


int getPayload(unsigned char *filename, uint64_t *timestampPCR, float *waitingtime, uint16_t *payload_len, unsigned char *payload, uint32_t allBytes){
    
    FILE *dataFile;
    uint8_t payloadTS[TSSIZE];
    uint8_t bytesR=0;
    uint32_t nTS=0;
    
    uint8_t afLen;
    uint16_t pid;
    unsigned char afc, afFlags, pcrFlag;
    uint64_t pcr_base;
        
    uint8_t fnewTime=0;
    dataFile=fopen(filename,"rb");
    if(dataFile==NULL){
        printf("No File!\n");
        return 2;
    }
    //printf("Here\n");
    if (fseek(dataFile,allBytes,SEEK_SET)>0){
        fclose(dataFile); 
        return 1;
    }

    *waitingtime=0;
    *payload_len=0;
    while(nTS<MAXTS && fnewTime==0){
        bytesR=fread(payloadTS,1,TSSIZE,(FILE*)dataFile);
    
        if(feof(dataFile)||bytesR<TSSIZE){
            fclose(dataFile);          
            return 1;
        }
        
        pid=(payloadTS[1]&0x1f)<<8|payloadTS[2];
        afc=(payloadTS[3]&0x30)>>4;
        //printf("PID: %u, AFC: %u\n",pid,afc);
        

        
        if(afc==2 || afc==3){
            afLen=payloadTS[4];
            afFlags=payloadTS[5];
            pcrFlag=(afFlags&0x10)>>4;
            //printf("AF length: %u, PCR flags: %02x\n",afLen,pcrFlag);
            if(pcrFlag & afLen>0){
                pcr_base=(payloadTS[6]<<25|payloadTS[7]<<17|payloadTS[8]<<9|payloadTS[9]<<1|(payloadTS[10]>>7));
                //printf("PCR: %lu\n",pcr_base); /////%u -> %lu
                
                if(pcr_base>*timestampPCR){
                    fnewTime=1;
                    *waitingtime=(float)(pcr_base-*timestampPCR)/90000;
                    //printf("Diff time: %lu (%f sec)\n",pcr_base-*timestampPCR,*waitingtime);
                    *timestampPCR=pcr_base;
                }
                
            }
            
         }
            
        if(fnewTime==0){
            memcpy(&payload[nTS*TSSIZE],payloadTS,TSSIZE);
            *payload_len+=TSSIZE;
            nTS++;
        }
    }

    
   
    if(feof(dataFile)){
        fclose(dataFile);          
        return 1;
    }
    //sleep(10);

    fclose(dataFile);  
    return 0; 
}


int sendStream(char *file, char *ip, int PORT) { 
    char filename[256],addr[16];
    
    uint8_t payload[MAXSIZE];
    int n=0,gout;
    uint64_t timestampPCR=0;
    uint32_t timestampRTP=50000;
    
    float waitingtime;
    uint16_t payload_len;
    uint32_t allBytes=0;
    
    int sockfd; 
    uint8_t buffer[MAXSIZE];
    struct sockaddr_in servaddr;  
    int len;
    
    RTPHeader rtph;
    uint32_t b0,b1,b2,b3;
    uint32_t sequencebe,timestampRTPbe;
    
    /*if(argc<3){
        printf("Usage:\n\t%s <filename.ts> <Dest.IPv4.Address>\n\n",argv[0]);
        exit(1);
    }
    */

    strcpy(filename,file);
    strcpy(addr,ip);
    //printf("%s ",filename);
    //printf("%ld\n",strlen(filename));

    if(inet_addr(addr)==-1){
        printf("Address is not in IPv4 format\n\n");
        exit(1);
    }

    printf("Streaming %s to %s\n",filename,addr);
    
    
    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
  
    memset(&servaddr, 0, sizeof(servaddr)); 
      
    // Filling server information 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(PORT); 
    servaddr.sin_addr.s_addr = inet_addr(addr);
       
 
    while(1){
        gout=getPayload(filename,&timestampPCR,&waitingtime,&payload_len,payload,allBytes);
        if(gout>0) return 0;
        
        timestampRTP+=(int)(waitingtime*90000);
        allBytes+=payload_len;
        
        //printf("%d-> Wait: %f sec, RTP time: %u, Payload %u bytes (%d TS)\n",n,waitingtime,timestampRTP,payload_len,(int)(payload_len/TSSIZE));
        n++;

        //Convert RTP sequence number and timestamp to Bog-Endian
        b0 = (n & 0x00ff) << 8u;
        b1 = (n & 0xff00) >> 8u;
        sequencebe = b0 | b1;
        
        b0 = (timestampRTP & 0x000000ff) << 24u;
        b1 = (timestampRTP & 0x0000ff00) << 8u;
        b2 = (timestampRTP & 0x00ff0000) >> 8u;
        b3 = (timestampRTP & 0xff000000) >> 24u;
        timestampRTPbe = b0 | b1 | b2 | b3;
        
        bzero(&rtph,RTPHEADERSIZE);
        rtph.version=2;
        rtph.pt=33;
        rtph.seq=sequencebe;
        rtph.ts=timestampRTPbe;
        rtph.ssrc=10;
        
        memcpy(buffer,&rtph,RTPHEADERSIZE);
        memcpy(&buffer[RTPHEADERSIZE],payload,payload_len);
        
        //printf("Waitting %u useconds\n",(unsigned int)(waitingtime*1e6));
        usleep((unsigned int)(waitingtime*1e6));
        sendto(sockfd, buffer,RTPHEADERSIZE+payload_len,MSG_CONFIRM, (const struct sockaddr *) &servaddr,sizeof(servaddr)); 
        //printf("Message sent.\n");
    }
    
    
    return 0; 
} 

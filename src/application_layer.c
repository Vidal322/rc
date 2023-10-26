// Application layer protocol implementation

//#include "application_layer.h"
#include "../include/link_layer.h"
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <math.h>

int createControlPacket(unsigned char c , unsigned char** controlPacketStart,  int fileSize, const char* filename){

    int filenameSize = strlen(filename) + 1; // number of octates of filename
    printf("Actual Filename: %d\n", filenameSize);
    int numBits = sizeof(int) * 8 - __builtin_clz(fileSize);
    unsigned char fileSizeSize = (numBits + 7) / 8;
// rounds up to the nearest byte
    unsigned char* controlPacket = (unsigned char*) malloc( filenameSize + fileSizeSize + 5); // allocs memory for the control packet
   
    
    controlPacket[0] = c;
    controlPacket[1] = 0x00;
    controlPacket[2] = filenameSize & 0xFF; // (char)filenameSize/sizeof(char)
    memcpy(controlPacket + 3, filename, filenameSize);
    
    controlPacket[3 + filenameSize] = 0x01;
    controlPacket[4 + filenameSize] = fileSizeSize; // rounds up to the nearest byte
    
    for (int i = 0; i < fileSizeSize; i++) { //n sei se aqui devo usar filesize +7 >>3
    controlPacket[4 + fileSizeSize + filenameSize - i] = (fileSize >> (i * 8)) & 0xFF;
}

    *controlPacketStart = controlPacket;
    

    return filenameSize + fileSizeSize + 5;
}


 int getData(unsigned char* dataHolder, FILE* file, int fileSize){
   if(fread(dataHolder,1,fileSize,file) != fileSize){
       printf("Error reading file\n");
       return -1;
   }
 }
// ver isto 
 int createDataPacket(unsigned char** dataHolder, unsigned char* dataPacket, int dataSize){
    // datapacketsize é suposto ser fixo?
    // n percebi pq tem de multiplicar por 256
    //é suposto estarmos sempre a criar um novo ou podemos reuitilizar memória?
    dataPacket[0] = 1;
    dataPacket[1] = 2;
    dataPacket[2] = dataSize >> 8 & 0xFF;
    dataPacket[3] = dataSize & 0xFF;
    memcpy(dataPacket + 4, *dataHolder,1000);
    *dataHolder += 1000;

 }

 int parseControlPacket(unsigned char* packet,  char** filename ,int* size){

    unsigned char filenameSize = packet[2];
    unsigned char fileSizeSize = packet[4 + filenameSize];

    char* fileName= (char*)malloc(filenameSize);
    unsigned char fileSize[fileSizeSize];

    memcpy(fileSize,packet + 5 + filenameSize,fileSizeSize);

    for (int i = 0; i < fileSizeSize; i++) {
        *size |= (fileSize[fileSizeSize - i - 1] << (i * 8));
    }
    for(int i = 3; i < 3 + filenameSize; i++)
        printf("%c", packet[i]);

    memcpy(fileName,packet + 3,filenameSize);
    for (int i = 0; i < filenameSize; i++) {
        printf("%c",fileName[i]);
    }

    *filename = fileName;
    return 0;
 }

 int parseDataPacket(unsigned char* packet,unsigned char* data, int size){

    memcpy(data,packet+4,size-4); //podemos mudar isto para usar a info de packet
    return 0;

 }

int sendFile(int fd,const char* filename){
    FILE* file = fopen(filename, "rb");
    
    if (file == NULL)
    {
        printf("Error opening file\n");
        return -1;
    }

    fseek(file, 0, SEEK_END);
     int fileSize = ftell(file);
    rewind(file);

    int controlPacketSize;
    unsigned char* controlPacketStart;
    controlPacketSize = createControlPacket(2 ,&controlPacketStart, fileSize, filename); // 2 for start control packet

    printf("Application Layer:\n");
    for(int i = 0; i< controlPacketSize; i++)
        printf("0x%02X ", controlPacketStart[i]);
    printf("\n");

    if(llwrite(controlPacketStart, controlPacketSize) == -1){
        printf("Error sending control packet\n");
        return -1;
    } // como é que vamos passar o fd?
    #define DATA_SIZE 1024
    // mantemos fixo?
    //definir o tamanho da info que vamos passar
    unsigned char* dataHolder = (unsigned char*)malloc(fileSize);
    getData(dataHolder,file,fileSize);
    int data_rest = fileSize % DATA_SIZE;
    int data_size;
    int packet_size;
    
    

    while(fileSize != 0){
        //create datapacket , filesize - data_size , senddatpacket
        data_size = fileSize > DATA_SIZE ? DATA_SIZE : data_rest;
        packet_size = data_size + 4;
        unsigned char* dataPacket = (unsigned char*)malloc(packet_size);
       
        createDataPacket(dataHolder,dataPacket,DATA_SIZE);
       
        if(llwrite(dataPacket,packet_size) == -1){
            printf("Error sending data packet\n");
            return -1;
        }

        fileSize -= data_size;
    }

    unsigned char* controlPacketEnd;
    controlPacketSize = createControlPacket(3 ,controlPacketEnd, fileSize, filename); // 2 for start control packet

    if(llwrite(controlPacketEnd, controlPacketSize) != 0){
        printf("Error sending control packet\n");
        return -1;
    }

    llclose(fd);
    
} 


void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{

    LinkLayer linkLayer;

       strcpy(linkLayer.serialPort,serialPort);
       linkLayer.baudRate = baudRate;
       linkLayer.nRetransmissions = nTries;
       linkLayer.timeout = 1;
       linkLayer.role = strcmp(role, "tx") == 0 ? LlTx : LlRx;
    

  
    int fd = llopen(linkLayer);
    if (fd < 0)
    {

        printf("Error opening connection\n");
        return;
    }
    
  

    switch(linkLayer.role)
    {
        case LlTx:
                   sendFile(fd, filename);
            break;
        case LlRx:
            receiveFile(fd);
            break;
    }      
}

int receiveFile(int fd){

    unsigned char* packet[DATA_SIZE];
    unsigned char* filename;
    int fileSize;
    int controlCheck = FALSE;
    int totalPackets;
    int packetsCount = 0;
    int actualPacketSize;
    llread(packet); 

    
    parseControlPacket(packet,&filename,&fileSize);
     int i = 0;
     
        printf("%s",filename[i]);
        i++;
        printf("%d",i);
    
    printf("%d",i);
    totalPackets = (fileSize + 1023) / 1024;
    
    FILE* receiverFile = fopen((char*) filename, "wb+");

    while(packetsCount < totalPackets){
        printf("%d\n",packetsCount);
        
        if (actualPacketSize = llread(packet) == -1)
            continue;
        unsigned char* data[actualPacketSize];

        parseDataPacket(packet,data,actualPacketSize);
        fwrite(data, sizeof(unsigned char),  actualPacketSize, receiverFile);
        packetsCount++;
    }
    while(llread(packet) == -1);
    
    if (parseControlPacket(packet,filename, fileSize) != -1);
    llclose(0);
    

    return 0;
}

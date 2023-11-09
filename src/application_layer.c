// Application layer protocol implementation
// Application layer protocol implementation

// Application layer protocol implementation

#include "application_layer.h"
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
#include "../include/link_layer.h"

#define DATA_SIZE 1000

const char* receiverFileName;

int createControlPacket(unsigned char c , unsigned char** controlPacketStart, long int fileSize, const char* filename){

    int filenameSize = strlen(filename) + 1; // number of octates of filename
    int numBits = sizeof(int) * 8 - __builtin_clz(fileSize);
    unsigned char fileSizeSize = (numBits + 7) / 8;
    unsigned char* controlPacket = (unsigned char*) malloc( filenameSize + fileSizeSize + 5); // allocs memory for the control packet
   
    
    controlPacket[0] = c;
    controlPacket[1] = 0x00;
    controlPacket[2] = filenameSize & 0xFF; 
    memcpy(controlPacket + 3, filename, filenameSize);

    
    controlPacket[3 + filenameSize] = 0x01;
    controlPacket[4 + filenameSize] = fileSizeSize;
    
    for (int i = 0; i < fileSizeSize; i++) { 
    controlPacket[4 + fileSizeSize + filenameSize - i] = (fileSize >> (i * 8)) & 0xFF;
}

    *controlPacketStart = controlPacket;
    
    return filenameSize + fileSizeSize + 5;
}

int getData(unsigned char* dataHolder, FILE* file, size_t fileSize){
   fread(dataHolder,sizeof(unsigned char),fileSize,file);
   return 0;
 }

unsigned char* createDataPacket(unsigned char* dataHolder, size_t dataSize){

    unsigned char* dataPacket = (unsigned char*)malloc(dataSize);
    dataPacket[0] = 1;
    dataPacket[1] = 2;
    dataPacket[2] = dataSize >> 8 & 0xFF;
    dataPacket[3] = dataSize & 0xFF;
    memcpy(dataPacket + 4, dataHolder, dataSize-4);
   
    return dataPacket;
 }

char* parseControlPacket(unsigned char* packet , long int* size){
   
    printf("parsedControlPacket\n");
        
    unsigned char filenameSize = packet[2];
    unsigned char fileSizeSize = packet[4 + filenameSize];

    char* fileName= (char*)malloc(filenameSize);
    unsigned char* fileSize = (unsigned char*)malloc(fileSizeSize);
  


    memcpy(fileSize,packet + 5 + filenameSize,fileSizeSize);
    
    for (int i = 0; i < fileSizeSize; i++) {
        *size |= (fileSize[fileSizeSize - i - 1] << (i * 8));
    }
    free(fileSize);
    memcpy(fileName,packet + 3,filenameSize);
    return fileName;

    return 0;
 }

 int parseDataPacket(unsigned char* packet,unsigned char* data, long int size){
    memcpy(data,packet + 4,size); //podemos mudar isto para usar a info de packet
    return 0;

 }

int sendFile(const char* filename){
    
    printf("File name: %s\n",filename);
   
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Error opening file\n");
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
    perror("fseek failed");
    // handle error
    }

    long int fileSize = ftell(file);

    if (fileSize == -1) {
        perror("ftell failed");
        // handle error
    }

    if (ferror(file)) {
        perror("Error occurred while accessing the file");
        // handle error
    }

    printf("FileSize: %ld\n\n", fileSize);
    rewind(file);

    int controlPacketSize;
    unsigned char* controlPacketStart;
    controlPacketSize = createControlPacket(2 ,&controlPacketStart, fileSize, filename); // 2 for start control packet
    
    printf("\nStart control packet\n");
    if(llwrite(controlPacketStart, controlPacketSize) == -1){
        printf("Error sending control packet\n");
        free(controlPacketStart);
        return -1;
    }
    free(controlPacketStart);

    
    unsigned char* dataHolder = (unsigned char*)malloc(fileSize);
    getData(dataHolder,file,fileSize);
    fclose(file);
    long int data_rest = fileSize % DATA_SIZE;
    long int data_size = 0;
    long int packet_size = 0;
    int packet_count = 0;
    
    long int fileSize2 = fileSize;

    
    while(fileSize2 != 0){
            
        //create datapacket , filesize - data_size , senddatpacket
        data_size = fileSize2 > DATA_SIZE ? DATA_SIZE : data_rest;

        packet_size = data_size + 4;
        unsigned char* dataPacket; 

        dataPacket = createDataPacket(dataHolder,packet_size);

        printf("\nPacket number: %i\n", packet_count++);

        if(llwrite(dataPacket,packet_size) == -1){
            printf("Error sending data packet\n");
            free(dataPacket);
            dataHolder -= (packet_count- 1)* DATA_SIZE;
            free(dataHolder);
            return -1;
        }

        dataHolder += data_size;
        fileSize2 -= data_size;
        free(dataPacket);
    }
    dataHolder -= fileSize;
    free(dataHolder);

    unsigned char* controlPacketEnd;
    controlPacketSize = createControlPacket(3 ,&controlPacketEnd, fileSize, filename); // 2 for start control packet
    printf("\nEnd control packet \n");

    if(llwrite(controlPacketEnd, controlPacketSize) == -1){
        printf("Error sending control packet\n");
        free(controlPacketEnd); 
        return -1;
    }
    free(controlPacketEnd);
    llclose(0);
    return 0;
    
} 

int receiveFile(){

    unsigned char packet[DATA_SIZE+5];
    char* filename;
    long int fileSize = 0;
    long int totalPackets = 0;
    int packetsCount = 0;
    long int actualPacketSize = 0;
    printf("\nStart control packet\n");
    llread(packet); 

    filename = parseControlPacket(packet,&fileSize);
   
    printf("file size: %li\n",fileSize);
    printf("file name: %s\n",filename);

    totalPackets = (fileSize + DATA_SIZE - 1) / DATA_SIZE;
    printf("TotalPackets: %li\n",totalPackets);
    
    FILE* receiverFile = fopen(receiverFileName, "wb+");
    if (receiverFile == NULL) {
        printf("Error opening file\n");
        return -1;
    }
    char* fn;
    while(packetsCount < totalPackets){
    
        printf("\nPacket Number: %d\n", packetsCount++);    
        if ((actualPacketSize = llread(packet)) == 0)
            continue;
        else if (actualPacketSize == -1) {
            printf("Error receiving data packet\n");
            free(filename);
            return -1;
        }

        actualPacketSize -= 5;  // Tirar 
        unsigned char *data = (unsigned char*) malloc(actualPacketSize);
        parseDataPacket(packet,data,actualPacketSize);

        fwrite(data, sizeof(unsigned char),  actualPacketSize, receiverFile);
        free(data);
    }
    fclose(receiverFile);
    printf("\nEnd control packet \n");

    while(llread(packet) == 0);
    fn = parseControlPacket(packet,&fileSize);
    if (strcmp(fn,filename) != 0) {
        printf("Error receiving control packet\n");
        printf("fn: %s\n", fn);
        printf("filename: %s\n", filename);
        free(filename);
        free(fn);
        return -1;
    }
    free(filename);
    free(fn);
    llclose(0);
    

    return 0;
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{

    LinkLayer linkLayer;

       strcpy(linkLayer.serialPort,serialPort);
       linkLayer.baudRate = baudRate;
       linkLayer.nRetransmissions = nTries;
       linkLayer.timeout = 3;
       linkLayer.role = strcmp(role, "tx") == 0 ? LlTx : LlRx;
    

    receiverFileName = filename;
  
    if (llopen(linkLayer) == - 1) {
        printf("Error opening connection\n");
        return;
    }

    switch(linkLayer.role)
    {
        case LlTx:
            sendFile(filename);
            break;
        case LlRx:
            receiveFile();
            break;
    } 

       
}



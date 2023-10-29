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

#define DATA_SIZE 256

int createControlPacket(unsigned char c , unsigned char** controlPacketStart, long int fileSize, const char* filename){

    int filenameSize = strlen(filename) + 1; // number of octates of filename
    printf("Actual FileSize: %ld\n", fileSize);
    printf("Actual FilenameSize: %i\n", filenameSize);
    
    int numBits = sizeof(int) * 8 - __builtin_clz(fileSize);
    unsigned char fileSizeSize = (numBits + 7) / 8;
    printf("FileSizeSize: %i\n",fileSizeSize);
    printf("packetSize: %i\n",filenameSize + fileSizeSize + 5);
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
    printf("controlPacket:");
    for(int i = 0; i < filenameSize + fileSizeSize + 5 ; i++){
        printf(" %02X ",controlPacket[i]);
    }
    printf("\n");
    printf("controlPacketStart:");
    for(int i = 0; i < filenameSize + fileSizeSize + 5 ; i++){
        printf(" %02X ",(*controlPacketStart)[i]);
    }
    printf("\n");
    

    return filenameSize + fileSizeSize + 5;
}

 int getData(unsigned char* dataHolder, FILE* file, size_t fileSize){
    
    printf("data file pointer 1: %p\n",file);
   fread(dataHolder,sizeof(unsigned char),fileSize,file);
   printf("data file pointer 2: %p\n",file);
       
   return 0;
 }
// ver isto 
 unsigned char* createDataPacket(unsigned char* dataHolder, size_t dataSize){
    // datapacketsize é suposto ser fixo?
    // n percebi pq tem de multiplicar por 256
    //é suposto estarmos sempre a criar um novo ou podemos reuitilizar memória?
    unsigned char* dataPacket = (unsigned char*)malloc(dataSize);
    printf("PacketSize: %ld  \n",dataSize);
    dataPacket[0] = 1;
    dataPacket[1] = 2;
    dataPacket[2] = dataSize >> 8 & 0xFF;
    dataPacket[3] = dataSize & 0xFF;
    memcpy(dataPacket + 4, dataHolder,dataSize-4);
    printf("Dataholder pointer: %p\n",dataHolder);
   

    return dataPacket;

 }

 char* parseControlPacket(unsigned char* packet , long int* size){
        printf("parsedControlPacket:");
       for(int i = 0; i < 20; i++){ 
         printf("%02X ", packet[i]);
       }
        
    unsigned char filenameSize = packet[2];
    unsigned char fileSizeSize = packet[4 + filenameSize];
    printf("fileSizeSize: %d\n",fileSizeSize);

    char* fileName= (char*)malloc(filenameSize);
    unsigned char* fileSize = (unsigned char*)malloc(fileSizeSize);
  

    memcpy(fileSize,packet + 5 + filenameSize,fileSizeSize);
    
        printf("sizee: %li\n",*size);
    for (int i = 0; i < fileSizeSize; i++) {
        *size |= (fileSize[fileSizeSize - i - 1] << (i * 8));
        printf("size: %li\n",*size);
        
    }
    
    memcpy(fileName,packet + 3,filenameSize);
    return fileName;
    
    /*for (int i = 0; i < filenameSize; i++) {
        printf("%c",filename[i]);
    }*/

    return 0;
 }

 int parseDataPacket(unsigned char* packet,unsigned char* data, long int size){
    printf("Cenas");
    memcpy(data,packet,size - 1); //podemos mudar isto para usar a info de packet
    return 0;

 }

int sendFile(int fd,const char* filename){
        printf("FileName:");
    for( int i = 0 ; i < 11; i++){
        printf("%c",filename[i]);
    }
    printf("\n");
   
    FILE* file = fopen(filename, "rb");
    
    if (file == NULL)
    {
        printf("Error opening file\n");
        return -1;
    }


    if (fseek(file, 0, SEEK_END) != 0) {
    perror("fseek failed");
    // handle error
}

long int fileSize = ftell(file);
printf("file inicio : %p\n",file);

if (fileSize == -1) {
    perror("ftell failed");
    // handle error
}

if (ferror(file)) {
    perror("Error occurred while accessing the file");
    // handle error
}

printf("FileSize: %ld\n", fileSize);
rewind(file);
printf("file fim : %p\n",file);



    int controlPacketSize;
    unsigned char* controlPacketStart;
    controlPacketSize = createControlPacket(2 ,&controlPacketStart, fileSize, filename); // 2 for start control packet
    printf("\ncontrolPacketSize: %d\n",controlPacketSize);
    printf("Application Layer:\n");

     for(int i = 0; i< controlPacketSize; i++)
        printf("%02X ", controlPacketStart[i]);
   
    printf("\n");

    if(llwrite(controlPacketStart, controlPacketSize) == -1){
        printf("Error sending control packet\n");
        return -1;
    } // como é que vamos passar o fd?
    
    // mantemos fixo?
   
    //definir o tamanho da info que vamos passar
    unsigned char* dataHolder = (unsigned char*)malloc(fileSize);
    getData(dataHolder,file,fileSize);
    long int data_rest = fileSize % DATA_SIZE;
    long int data_size = 0;
    long int packet_size = 0;
    
    
    int fileSize2 = fileSize;
    while(fileSize2 != 0){
            
        //create datapacket , filesize - data_size , senddatpacket
        data_size = fileSize2 > DATA_SIZE ? DATA_SIZE : data_rest;
        printf("DataHolder :%li\n",fileSize2);
        printf("data_size: %li\n",data_size);
        packet_size = data_size + 4;
        printf("packet_size: %li\n",packet_size);
        unsigned char* dataPacket; //(unsigned char*)malloc(packet_size);
        printf("dataHolder :\n");
        for(int i = 0; i < 30; i++){
            printf("%02X ",dataHolder[i]);
        }
        printf("\n");
        dataPacket = createDataPacket(dataHolder,packet_size);
        /*printf("packet_size: %d\n",packet_size);
        for(int i = 0; i < packet_size; i++){
            printf("%02X ",dataPacket[i]);
        }
        printf("\n");*/
        printf("dataPacket pointer: %p\n",dataPacket);
        if(llwrite(dataPacket,packet_size) == -1){
            printf("Error sending data packet\n");
            return -1;
        }
        dataHolder+=data_size;
        fileSize2 -= data_size;
    }

    unsigned char* controlPacketEnd;
    controlPacketSize = createControlPacket(3 ,&controlPacketEnd, fileSize, filename); // 2 for start control packet

    if(llwrite(controlPacketEnd, controlPacketSize) == -1){
        printf("Error sending control packet\n");
        return -1;
    }

    llclose(fd);
    return 0;
    
} 

int receiveFile(int fd){

    unsigned char packet[DATA_SIZE+9];
    char* filename;
    long int fileSize = 0;
    long int totalPackets = 0;
    long int packetsCount = 0;
    long int actualPacketSize = 0;
    llread(packet); 

    printf("packet:");
    for(int i = 0 ; i < 19; i++){
        printf("%02X ",packet[i]);
    }
    printf("\n");

    filename = parseControlPacket(packet,&fileSize);
   
     printf("\nfileSize: %li",fileSize);
     printf("\nfileName:");
    for (int i = 0; i < 11; i++) {
        printf("%c",filename[i]);
    }
    printf("\nfileSize: %li\n",fileSize);
    totalPackets = (fileSize + DATA_SIZE - 1) / DATA_SIZE;
    printf("\nTotalPackets: %li\n",totalPackets);
    
    FILE* receiverFile = fopen(filename, "wb+");
    if (receiverFile == NULL)
    {
        printf("Error opening file\n");
        return -1;
    }
    char* fn;
    while(packetsCount < totalPackets){
        printf("packet count : %li\n",packetsCount);
    
        if ((actualPacketSize = llread(packet)) == -1){
            printf("Error receiving data packet\n");
            return -1;
        }
        
        if ((actualPacketSize = llread(packet)) == 0)
            continue;
        unsigned char *data = (unsigned char*) malloc(actualPacketSize);
        parseDataPacket(packet,data,actualPacketSize);
        packetsCount++;

        printf("\nparsed the Data\n");
        fwrite(data, sizeof(unsigned char),  actualPacketSize, receiverFile);
        free(data);
        if (packet[0] == 3) {
            printf("End Packet\n");   
                fn = parseControlPacket(packet,&fileSize);      
            if (strcmp(fn,filename) != 0){
                printf("filenameOnClose: %s\n",fn);
                printf("filename: %s\n",filename);
                printf("different filenames: %d\n",strcmp(fn,filename));
                printf("Error receiving control packet\n");
                return -1;
            }
            break;
        }
    }
    printf("here\n");
    /*if (parseControlPacket(packet, &fileSize) != filename)
    {
        printf("Error receiving control packet\n");
        return -1;
    }*/

    printf("Vai mandar para o llclose\n");
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
       linkLayer.timeout = 1;
       linkLayer.role = strcmp(role, "tx") == 0 ? LlTx : LlRx;
    

  
    int fd = llopen(linkLayer);
    if (fd < 0)
    {

        printf("Error opening connection\n");
        return;
    }
    
  printf("Ceans");

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



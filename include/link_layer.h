// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

#include <signal.h>
#include <termios.h>
typedef enum
{
    LlTx,
    LlRx,
} LinkLayerRole;

typedef struct
{
    char serialPort[50];
    LinkLayerRole role;
    int baudRate;
    int nRetransmissions;
    int timeout;
} LinkLayer;

// SIZE of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 1000

// MISC
#define FALSE 0
#define TRUE 1

#define BUF_SIZE 2048
#define CONTROL_BYTE_SIZE 5

#define O_WRONLY	     01
#define O_RDWR		     02
#define O_NOCTTY	   0400


#define FLAG 0x7E
#define FLAG_COVER 0x7D
#define FLAG_TAG 0x5E
#define FLAG_COVER_TAG 0x5D

// Field A
#define A_tx 0x03  //tx commands / rx responses
#define A_rx 0x01  //rx commands / tx responses
// Field C
#define C_SET 0x03
#define C_UA 0x07
#define C_RR0 0x05  // sent by rx -> ready to receive frame number 0
#define C_RR1 0x85  // sent by rx -> ready to receive frame number 1
#define C_REJ0 0x01 // sent by rx -> rejected frame number 0
#define C_REJ1 0x81 // sent by rx -> rejected frame number 1
#define C_RR(s) (0x05 | ((1 << 7) * s))
#define C_REJ(s) (0x01 | ((1 << 7) * s))
#define C_DISC 0x0B 
#define C_F1 0x00  // Frame number 0
#define C_F2 0x40  // Frame number 1
#



// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(LinkLayer connectionParameters);


// connects to a serial port 
// returns -1 or fd on success 
int openSerialPort(char* serialPort, int baudrate);

unsigned char readResponse();

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(const unsigned char *buf, int bufSize);

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(unsigned char *packet);

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int showStatistics);

#endif // _LINK_LAYER_H_

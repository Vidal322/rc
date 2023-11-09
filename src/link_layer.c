#include "link_layer.h"
#include <signal.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>


#define MAX_PAYLOAD_SIZE 1000

// MISC
#define FALSE 0
#define TRUE 1

#define BUF_SIZE 2048
#define CONTROL_BYTE_SIZE 5

#define O_WRONLY	     01
//#define O_RDWR		     02
//#define O_NOCTTY	   0400


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



// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source


int alarmEnabled = FALSE;
int alarmCount = 0;
volatile int STOP = FALSE;
int retransmitions;
int timeout;
int fd;
int tx_frame_index = 0;
int rx_frame_index = 0;
LinkLayerRole gRole;
struct termios oldtio;

// ---------- Stats ---------
int framesSent = 0;
int framesReceived = 0;
int control_bytes_sent = 0;
int retries = 0;
clock_t start_tx, end_tx,start_rx, end_rx;
double time_tx= 0;
double time_rx = 0;
int bytes_sent = 0;
int bytes_received = 0;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////

void changeOpenState(unsigned char buf, int* state, LinkLayerRole role){
  
    if (role == LlTx)
    switch(*state) {
        case 0:
            if(buf == FLAG) {
                *state = 1;
            }
            break;
        case 1:
            if(buf == A_tx) {
                *state = 2;
            } else if(buf != FLAG) {
                *state = 0;
            } else *state = 1;
            break;
        case 2:
            if(buf == C_UA) {
                *state = 3;
            } else if(buf != 0x7E) {
                *state = 0;
            } else *state = 1;
            break;
        case 3:
            if(buf == (A_tx ^ C_UA)) {
                *state = 4;
            } else if(buf != FLAG) {
                *state = 0;
            } else *state = 1;
            break;
        case 4:
            if(buf == FLAG) {
                *state = 5;
            } else {
                *state = 0;
            }
            break;
        default:
            *state = 0;
            break;
    }

    else if (role == LlRx) {
         switch(*state) {
        case 0:
            if(buf == FLAG) {
                *state = 1;
            }
            break;
        case 1:
            if(buf == A_tx) {
                *state = 2;
            } else if(buf != 0x7E) {
                *state = 0;
            } else *state = 1;
            break;
        case 2:
            if(buf == C_SET) {
                *state = 3;
            } else if(buf != FLAG) {
                *state = 0;
            } else *state = 1;
            break;
        case 3:
            if(buf == (A_tx ^ C_SET)) {
                *state = 4;
            } else if(buf != FLAG) {
                *state = 0;
            } else *state = 1;
            break;
        case 4:
            if(buf == FLAG) {
                *state = 5;
            } else {
                *state = 0;
            }
            break;
        default:
            *state = 0;
            break;
    }
    }
}

int openSerialPort(char* serialPort, int baudrate) {  

    fd = open(serialPort, O_RDWR | O_NOCTTY);
    
    printf("Serial Port: %d\n",fd);
    if (fd < 0) {
        perror(serialPort);
        exit(-1);
    }
    
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }


    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = baudrate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 1;
    newtio.c_cc[VMIN] = 0; 


    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");
    return fd;
}

int llopen(LinkLayer connectionParameters){
    STOP = FALSE;
    alarmEnabled = FALSE;
    alarmCount = 0;
    int state = 0;
    fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate);
    timeout = connectionParameters.timeout;
    retransmitions = connectionParameters.nRetransmissions;
    unsigned char set[CONTROL_BYTE_SIZE];
    unsigned char ua[CONTROL_BYTE_SIZE];
    gRole = connectionParameters.role;

    set[0] = FLAG;
    set[1] = A_tx;
    set[2] = C_SET;
    set[3] = set[1] ^ set[2];
    set[4] = FLAG;

    ua[0] = FLAG;
    ua[1] = A_tx;
    ua[2] = C_UA;
    ua[3] = ua[1] ^ ua[2];
    ua[4] = FLAG;

    // Create string to send
    unsigned char buf[CONTROL_BYTE_SIZE] = {0};


    if (connectionParameters.role == LlTx) {
    (void)signal(SIGALRM, alarmHandler);
    
    while (alarmCount < retransmitions) {
        
        if (alarmEnabled == FALSE) {
            write(fd, set, CONTROL_BYTE_SIZE);
            control_bytes_sent += CONTROL_BYTE_SIZE;
            alarmEnabled = TRUE;
            alarm(timeout); 
            state = 0;
        }
        if (alarmCount == retransmitions)
            break;

        if(read(fd, buf, 1) == 0) continue;

        changeOpenState(buf[0], &state, connectionParameters.role);

        if (state == 5) {
            alarm(0);
            break;
        }
    }
    start_tx = clock();

    if (state != 5) {
        return -1;
    }
    printf("llopen\n");
    return 1;
    }


    else {  // Receiver

    unsigned char buf[CONTROL_BYTE_SIZE] = {0}; 
    int state = 0;

    while (STOP == FALSE) {
        if (read(fd, buf, 1) == 0)
            continue;

        changeOpenState(buf[0], &state, connectionParameters.role);
        //printf("var = 0x%02X  state = %d \n", (unsigned int)(buf[0] & 0xFF), state);
        
        if (state == 5)
            STOP = TRUE;
    }
    write(fd, ua, CONTROL_BYTE_SIZE);
    control_bytes_sent += CONTROL_BYTE_SIZE;
    printf("sent acknowledge package\n");
    start_rx = clock();

    }
    return 0;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int specialByteCount(const unsigned char* data, int size) {
    int total = 0;
    for(int i = 0; i < size; i++)
        if ((data[i] == 0x7E) | (data[i] == 0x7D))
            total++;
    return total;
}

int stuffArray(const unsigned char* data,unsigned char* res, int size) {
    int j = 0, i = 0;
    //printf("data:\n");
    for (i = 0; i < size; i++) {
        //printf("%02x ", data[i]);
        if (data[i] == 0x7E) {
            res[j++] = 0x7D;
            res[j] = 0x5E;
        }
        else if (data[i] == 0x7D) {
            res[j++] = 0x7D;
            res[j] = 0x5D;
        }
        else
            res[j] = data[i];
        j++;
    }
    return 0;
}

unsigned char calc_BBC_2 (const unsigned char* data, unsigned char* newData, int size) {
    unsigned char res = data[0];
    newData[0] = data[0];
    for (int i = 1; i  < size; i++){
        res ^= data[i];
        newData[i] = data[i];
    }
    //printf("BBC_2 : %02X\n",res);

    newData[size] = res;

    return 0;
}

int changeControlPacketState(unsigned char buf, int* state, unsigned char* byte) {
    switch(*state) {
        case 0:
            if(buf == FLAG) {
                *state = 1;
            }
            break;
        case 1:
            if(buf == A_tx) {
                *state = 2;
            } else if(buf != FLAG) {
                *state = 0;
            } else *state = 1;
            break;
        case 2:
            if(buf == C_RR0 || buf == C_RR1 || buf == C_REJ0 || buf == C_REJ1) {
                *state = 3;
                *byte = buf;
            } else if(buf != FLAG) {
                *state = 0;
            } else *state = 1;
            break;
        case 3:
            if(buf == (A_tx ^ *byte)) { 
                *state = 4;
            } else if(buf != FLAG) {
                *state = 0;
            } else *state = 1;
            break;
        case 4:
            if(buf == FLAG) {
                *state = 5;
            } else {
                *state = 0;
            }
            break;
        default:
            *state = 0;
            break;
    }
    return 0;
}

int llwrite(const unsigned char *buf, int bufSize){

    unsigned char newData[bufSize + 1];
    calc_BBC_2(buf,newData, bufSize);

    int payload_size = bufSize + 1 + specialByteCount(newData, bufSize + 1);
    unsigned char payload[payload_size];
    int frame_size = 5 + payload_size;
    unsigned char frame[frame_size];

    if (payload_size != bufSize + 1) {
         stuffArray(newData, payload, bufSize + 1);
     }         // eficiencia
     else {
        memcpy(payload,newData,bufSize+1);
     }

    frame[0] = FLAG;                // First Flag
    frame[1] = A_tx;                // A
    frame[2] = tx_frame_index * 0x40;  // N(s)
    frame[3] = frame[1] ^ frame[2]; // BCC1 
    memcpy(frame + 4, payload, payload_size);
    frame[frame_size - 1] = FLAG;   // Final Flag

    alarmCount = 0;
    alarmEnabled = FALSE;
    int state = 0;
    unsigned char byte = 0x00;
    (void)signal(SIGALRM, alarmHandler);
    unsigned char buffer[1];
    int written_bytes = 0;


    while(alarmCount < retransmitions ){ 
    if(alarmEnabled == FALSE){ 
        written_bytes = write(fd, frame, frame_size);
        if (alarmCount == 0) {
            bytes_sent += frame_size;
            framesSent++;
        }
        else {
            retries++;
        }
        alarmEnabled = TRUE;
        alarm(timeout);
        }
         if (read(fd, buffer, 1) == 0)
            continue;
        changeControlPacketState(buffer[0], &state, &byte);
        if (state != 5)
            continue;
        if (byte == C_RR0 || byte == C_RR1) {
            printf("RR\n");
            tx_frame_index ++;
            tx_frame_index %= 2;
            return written_bytes;
        }
        if (byte == C_REJ0 || byte == C_REJ1)  { 
            printf("REJ\n");
            alarmEnabled = FALSE;
            }
    }
    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////

int send_RR (unsigned char frame_index){
    
    unsigned char RR[5];

    RR[0] = FLAG;
    RR[1] = A_tx;
    RR[2] = C_RR(frame_index);
    RR[3] = RR[1] ^ RR[2];
    RR[4] = FLAG;

    write(fd,RR,5);
    control_bytes_sent += 5;
    return 0;
}

int send_REJ (unsigned char frame_index){

    unsigned char REJ[5];

   REJ[0] = FLAG;
   REJ[1] = A_tx;
   REJ[2] = C_REJ(frame_index);
   REJ[3] = REJ[1] ^REJ[2];
   REJ[4] = FLAG;

    write(fd,REJ,5);
    control_bytes_sent += 5;
    return 0;
}

int send_Rx_DISC () {
    unsigned char rxDisc[5];
    rxDisc[0] = FLAG;
    rxDisc[1] = A_rx;
    rxDisc[2] = C_DISC;
    rxDisc[3] = rxDisc[1] ^ rxDisc[2];
    rxDisc[4] = FLAG;

    write(fd, rxDisc, 5);
    control_bytes_sent += 5;
    return 0;
}

int changeReadState(unsigned char buf, int* state, unsigned char* packet, int* index){

    switch(*state) { 
        case 0:
            *index = 0;
            if(buf == FLAG) {
                *state = 1;
            }
            break;
        case 1:              // Received Flag
            *index = 0;           
            if(buf == A_tx) { 
                *state = 2;
            } 
            else if(buf != FLAG) {
                *state = 0;
            } 
            else { 
                *state = 1;
            }
            break;
        case 2:                 // Received A_tx
            if(buf == (0x40* rx_frame_index)) {
                *state = 3;     
                }
            else if (buf == 0x40 * ((rx_frame_index + 1) % 2)) {
                send_RR((rx_frame_index));
                return 0;
            }
            else if(buf != FLAG) {
                *state = 0;
            }
            else {
                *state = 1;
                }
            break;
        case 3:                     // Received N(s) 
            if(buf == (A_tx ^ 0x40)|| buf == (A_tx ^ 0x00)) {
                *state = 4;
            } else if(buf != FLAG) {
                *state = 0;
            } else {
                *state = 1;
                }
            break;
        case 4:                 // Header Correct -> Reading Packet
            if (buf == FLAG_COVER) {
                *state = 5;
            }
            else if (buf == FLAG) {
                *state = 6;
                unsigned char bcc2 = packet[(*index)-1];
                unsigned char xor = packet[0];

                for (int j = 1; j< *index-1; j++) {
                    xor ^= packet[j];
                }                                             // 
                if (bcc2 == xor) {  // Valid data
                    *state = 6;
                    rx_frame_index = (rx_frame_index + 1) % 2;
                    send_RR(rx_frame_index);
                    printf("SENT RR\n");
                    return 0;
                }
                else  {
                    send_REJ(rx_frame_index);
                    printf("SENT REJ\n");
                    *state = 0;
                    *index = 0;
                    return -1;
                }
            }
            else {
                packet[(*index)++] = buf;
                
            }
            break;
        case 5:                         // Received 0x7D
            bytes_received ++;
            if (buf == FLAG_COVER_TAG) {
                packet[(*index)++] = FLAG_COVER;
                *state = 4;
            }
            else if (buf == FLAG_TAG) {
                packet[(*index)++] = FLAG;
                *state = 4;
            }
            else if (buf == FLAG) {
                packet[(*index)++] = FLAG_COVER;
                *state = 6;
            }
            else {
                packet[(*index)++] = FLAG_COVER;
                packet[(*index)++] = buf;
                *state = 4;
            }
            break;
        default:
            *state = 0;
            break;
    }
    return 0;
}

int llread(unsigned char *packet) {
    int state = 0;
    unsigned char buf[1];
    int size = 0;
    while(state != 6) {
        if (read(fd, buf, 1) == 0)
            continue;
        changeReadState(buf[0], &state, packet, &size);
        if (size == -1)
            return -1;
    }
    bytes_received += size + 5;
    framesReceived++;
    return size;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////

void changeCloseStateTx(unsigned char buf, int* state) {

    switch(*state) {
        case 0:
            if(buf == FLAG) {
                *state = 1;
            }
            break;
        case 1:
            if(buf == A_rx) {
                *state = 2;
            } else if(buf != FLAG) {
                *state = 0;
            } else *state = 1;
            break;
        case 2:
            if(buf == C_DISC) {
                *state = 3;
            } else if(buf != FLAG) {
                *state = 0;
            } else *state = 1;
            break;
        case 3:
            if(buf == (A_rx ^ C_DISC)) {
                *state = 4;
            } else if(buf != FLAG) {
                *state = 0;
            } else *state = 1;
            break;
        case 4:
            if(buf == FLAG) {
                *state = 5;
            } else {
                *state = 0;
            }
            break;
        default:
            *state = 0;
            break;
    }
}

void changeCloseStateRxDisc(unsigned char buf, int* state){

    switch(*state) {
        case 0:
            if(buf == FLAG) {
                *state = 1;
            }
            break;
        case 1:
            if(buf == A_tx) {
                *state = 2;
            } else if(buf != FLAG) {
                *state = 0;
            } else *state = 1;
            break;
        case 2:
            if(buf == C_DISC) {
                *state = 3;
            } else if(buf != FLAG) {
                *state = 0;
            } else *state = 1;
            break;
        case 3:
            if(buf == (C_DISC ^ A_tx)) {
                *state = 4;
            } else if(buf != FLAG) {
                *state = 0;
            } else *state = 1;
            break;
        case 4:
            if(buf == FLAG) {
                *state = 5;
            } else {
                *state = 0;
            }
            break;
       

        default:
            *state = 0;
            break;
    }
}

void changeCloseStateRxUa(unsigned char buf, int* state){

    switch(*state) {
        case 0:
            if(buf == FLAG) {
                *state = 1;
            }
            break;
        case 1:
            if(buf == A_rx) {
                *state = 2;
            } else if(buf != 0x7E) {
                *state = 0;
            } else *state = 1;
            break;
        case 2:
            if(buf == C_UA) {
                *state = 3;
            } else if(buf != FLAG) {
                *state = 0;
            } else *state = 1;
            break;
        case 3:
            if(buf == (C_UA ^ A_rx)) {
                *state = 4;
            } else if(buf != FLAG) {
                *state = 0;
            } else *state = 1;
            break;
        case 4:
            if(buf == FLAG) {
                *state = 5;
            } else {
                *state = 0;
            }
            break;
       

        default:
            *state = 0;
            break;
    }
}
int print_stats(){
    printf("Frames Sent: %d\n", framesSent);
    printf("Frames Received: %d\n", framesReceived);
    printf("Control Bytes Sent: %d\n", control_bytes_sent);
    printf("Retries: %d\n", retries);
    printf("Bytes Sent: %d\n", bytes_sent);
    printf("Bytes Received: %d\n", bytes_received);
    printf("Time Tx: %f\n", time_tx);
    printf("Time Rx: %f\n", time_rx);
    return 0;

}

int llclose(int showStatistics) {
    end_tx = clock();
    time_tx = (double)((end_tx - start_tx) / CLOCKS_PER_SEC * 1000); 

    end_rx = clock();
    time_rx = (double)((end_rx - start_rx) / CLOCKS_PER_SEC * 1000);

    int state = 0;
    unsigned char tx_disc[BUF_SIZE];
    unsigned char ua[BUF_SIZE];
    unsigned char rx_disc[BUF_SIZE];
    tx_disc[0] = FLAG;
    tx_disc[1] = A_tx;
    tx_disc[2] = C_DISC;
    tx_disc[3] = tx_disc[1] ^ tx_disc[2];
    tx_disc[4] = FLAG;
    
    rx_disc[0] = FLAG;
    rx_disc[1] = A_rx;
    rx_disc[2] = C_DISC;
    rx_disc[3] = rx_disc[1] ^ rx_disc[2];
    rx_disc[4] = FLAG;

    ua[0] = FLAG;
    ua[1] = A_rx;
    ua[2] = C_UA;
    ua[3] = ua[1] ^ ua[2];
    ua[4] = FLAG;  

    alarmCount = 0;
    alarmEnabled = FALSE;

    unsigned char buf[BUF_SIZE] = {0};
    (void)signal(SIGALRM, alarmHandler);

    if (gRole == LlTx) {                    

    while (alarmCount < retransmitions) {
        
        if (alarmEnabled == FALSE) {
            write(fd, tx_disc, CONTROL_BYTE_SIZE);
            control_bytes_sent += CONTROL_BYTE_SIZE;
            alarmEnabled = TRUE;
            alarm(timeout);
            state = 0;
        }

        if (read(fd, buf, 1) == 0) continue;
        changeCloseStateTx(buf[0], &state);

        if (state == 5) {
            alarm(0);
            write(fd, ua, CONTROL_BYTE_SIZE);
            control_bytes_sent += CONTROL_BYTE_SIZE;
            break;
        }
    }
    }

    else {

        STOP = FALSE;
        state = 0;
        while (STOP == FALSE) {
            if (read(fd, buf, 1) == 0)
                continue;
            changeCloseStateRxDisc(buf[0],&state);
            
            if (state == 5) {
                STOP = TRUE;
                           }
        }
        alarmCount = 0;
        alarmEnabled = FALSE;
        while (alarmCount < retransmitions) {
        if (alarmEnabled == FALSE) {
            write(fd, rx_disc, CONTROL_BYTE_SIZE);
            control_bytes_sent += CONTROL_BYTE_SIZE;
            alarmEnabled = TRUE;
            alarm(timeout);
            state = 0;
            alarmCount == 1 ? framesSent++ : retries++;

        }
        if (alarmCount == 3)
            break;

        if (read(fd, buf, 1) == 0) continue;
        
        changeCloseStateRxUa(buf[0], &state);
        
        if (state == 5) {
            alarm(0);
 
            break;
        }
    }
    }
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1){
        perror("tcsetattr");
        exit(-1);
    }
    close(fd);
    if (state == 5) {
        print_stats();
        printf("llclose\n");
        return 1;
    }

    return -1;
}

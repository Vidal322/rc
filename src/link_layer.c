// Link layer protocol implementation

#include "link_layer.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source


int alarmEnabled = FALSE;
int alarmCount = 0;
volatile int STOP = FALSE;
int retransmitions;
int timeout;
int tx_frame_index = 0;
int rx_frame_index = 0;
int fd;
LinkLayerRole gRole;

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
    LinkLayerRole grole = gRole;
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
            if(buf == A_tx ^ C_UA) {
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
            if(buf == A_tx ^ C_SET) {
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

    int fd = open(serialPort, O_RDWR | O_NOCTTY);
    
    printf("%d",fd);
    if (fd < 0) {
        perror(serialPort);
        exit(-1);
    }
    

    struct termios oldtio;
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
    newtio.c_cc[VTIME] = 1; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received



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
            alarmEnabled = TRUE;
            alarm(timeout); 
            state = 0;
        }
        if (alarmCount == retransmitions)
            break;

        read(fd, buf, 1);

        //printf("var = 0x%02X state:%d\n", (unsigned int)(buf[0] & 0xFF), state);
        changeOpenState(buf[0], &state, connectionParameters.role);

        if (state == 5) {
            alarm(0);
            break;
        }
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
    printf("sent acknowledge package\n");
    }
    return 0;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int specialByteCount(unsigned char* data, int size) {
    int total = 0;
    for(int i = 0; i < size; i++)
        if (data[i] == 0x7E | data[i] == 0x7D)
            total++;
    return total;
}

int stuffArray(unsigned char* data,unsigned char* res, int size) {
    int j = 0, i = 0;
    printf("data:\n");
    for (i = 0; i < size; i++) {
        printf("0x%02x", data[i]);
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

unsigned char calc_BBC_2 (unsigned char* data, unsigned char* newData, int size) {
    unsigned char res = data[0];
    newData[0] = data[0];
    for (int i = 1; i  < size; i++){
        res ^= data[i];
        newData[i] = data[i];
    }
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
            } else if(buf != 0x7E) {
                *state = 0;
            } else *state = 1;
            break;
        case 3:
            if(buf == A_rx ^ *byte) {
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
    buf = newData;

    int payload_size = bufSize + 1 + specialByteCount(buf, bufSize + 1);
    unsigned char payload[payload_size];
    int frame_size = 5 + payload_size;
    unsigned char frame[frame_size];

    if (payload_size != bufSize + 1) {
         stuffArray(buf, payload, bufSize + 1);
         
       
     }         // eficiencia
     else {
        memcpy(payload,buf,bufSize+1);
     }

    frame[0] = FLAG;                // First Flag
    frame[1] = A_tx;                // A
    frame[2] = tx_frame_index * 0x40;  // N(s)
    frame[3] = frame[1] ^ frame[2]; // BCC1 
    for(int i = 0; i < payload_size; i++) { // payload
        frame[4 + i] = payload[i];
    }
    frame[frame_size - 1] = FLAG;   // Final Flag


    alarmCount = 0;
    alarmEnabled = FALSE;
    int state = 0;
    unsigned char byte = 0x00;
    (void)signal(SIGALRM, alarmHandler);

    while(alarmCount < 3 ){ 
    if(alarmEnabled == FALSE){ 
        write(fd, frame, frame_size);
        alarmEnabled = TRUE;
        alarm(timeout);
        }
         if (read(fd, buf, 1) == 0)
            continue;

        changeControlPacketState(buf[0], &state, &byte);
        if (byte == C_RR0 || byte == C_RR1) {
            printf("RR\n");
            tx_frame_index ++;
            tx_frame_index %= 2;
            return frame_size;
        }
        if (byte == C_REJ0 || byte == C_REJ1)  { 
            printf("REJ\n");
            return -1; 
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
    return 0;
}

int send_REJ (unsigned char frame_index){

    unsigned char RR[5];

    RR[0] = FLAG;
    RR[1] = A_tx;
    RR[2] = C_REJ(frame_index);
    RR[3] = RR[1] ^ RR[2];
    RR[4] = FLAG;

    write(fd,RR,5);
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
            if(buf == 0x40* rx_frame_index) { // Correto
                *state = 3;     
                }
            else if (buf == 0x40 * ((rx_frame_index + 1) % 2)) {
                send_RR((rx_frame_index + 1) % 2);
                return 0;
            }
            else if(buf != FLAG) {
                *state = 0;

            } else {
                *state = 1;
                }
            break;
        case 3:                     // Received N(s) 
            if(buf == A_tx ^ 0x40|| buf == A_tx ^ 0x00) {
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
                unsigned char bcc2 = packet[*index];
                unsigned char xor = packet[0];
                for (int j = 1; j< *index; j++) {
                    xor ^= packet[j];
                }
                if (bcc2 == xor) {  // Valid data
                    *state = 6;
                    send_RR((rx_frame_index + 1) % 2);
                    printf("SENT RR\n");
                    rx_frame_index = (rx_frame_index + 1) % 2;
                    return *index;
                }
                else  {
                    send_REJ(rx_frame_index);
                    printf("SENT REJ\n");
                    return -1;
                }
            }
            else {
                packet[(*index)++] = buf;
                
            }
            break;
        case 5:                         // Received 0x7D
            if (buf == FLAG_COVER_TAG) {
                packet[(*index)++] = FLAG_COVER;
            }
            else if (buf == FLAG_TAG) {
                packet[(*index)++] = FLAG;
            }
            else if (buf == FLAG) {
                packet[(*index)++] = FLAG_COVER;
                *state = 6;
            }
            else {
                packet[(*index)++] = FLAG_COVER;
                packet[(*index)++] = buf;
            }
            break;
        default:
            *state = 0;
            break;
    }
    return index;
}


int llread(unsigned char *packet) {
    int state = 0;
    int i = 0;
    unsigned char buf[1];
    int size = 0;

    while(state != 6) {
        if (read(fd, buf, 1) == 0)
            continue;
        changeReadState(buf[0], &state, packet, &size);

        if (size == -1)
            return -1;
    }
   
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
            if(buf == A_rx ^ C_DISC) {
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
            } else if(buf != 0x7E) {
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
            if(buf == C_DISC ^ A_tx) {
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
            if(buf == C_UA ^ A_rx) {
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

int llclose(int showStatistics) {
    int state = 0;
    unsigned char tx_disc[BUF_SIZE];
    unsigned char ua[BUF_SIZE];
    tx_disc[0] = FLAG;
    tx_disc[1] = A_tx;
    tx_disc[2] = C_DISC;
    tx_disc[3] = tx_disc[1] ^ tx_disc[2];
    tx_disc[4] = FLAG;

    ua[0] = FLAG;
    ua[1] = A_tx;
    ua[2] = C_UA;
    ua[3] = ua[1] ^ ua[2];
    ua[4] = FLAG;  

    alarmCount = 0;
    alarmEnabled = FALSE;

    unsigned char buf[BUF_SIZE] = {0};
    (void)signal(SIGALRM, alarmHandler);

    if (gRole == LlTx) {                    

    while (alarmCount < 3) {
        
        if (alarmEnabled == FALSE) {
            write(fd, tx_disc, BUF_SIZE);
            alarmEnabled = TRUE;
            alarm(3);
            state = 0;
        }
        if (alarmCount == 3)
            break;

        read(fd, buf, 1);

        //printf("var = 0x%02X state:%d\n", (unsigned int)(buf[0] & 0xFF), state);
        changeCloseStateTx(buf[0], &state);

        if (state == 5) {
            alarm(0);
            write(fd, ua, BUF_SIZE);
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
                break;
                           }
        }
        alarmCount = 0;
        alarmEnabled = FALSE;
        while (alarmCount < 3) {
        
        if (alarmEnabled == FALSE) {
            send_Rx_DISC();
            alarmEnabled = TRUE;
            alarm(3);
            state = 0;
        }
        if (alarmCount == 3)
            break;

        read(fd, buf, 1);

        //printf("var = 0x%02X state:%d\n", (unsigned int)(buf[0] & 0xFF), state);
        changeCloseStateRxUa(buf[0], &state);

        if (state == 5) {
            alarm(0);
            break;
        }
    }
    }
    close(fd);
    if (state == 5) {
        //print_stats();
        return 1;
    }
    return -1;
}
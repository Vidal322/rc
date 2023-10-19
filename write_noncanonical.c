#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>


// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source
#define Start 0
#define Flag_S 1
#define A 2
#define C 3
#define BCC 4 
#define Flag_E 5
#define Stop 6

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 5

volatile int STOP = FALSE;


#define FALSE 0
#define TRUE 1

int alarmEnabled = FALSE;
int alarmCount = 0;
int state = 0;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

void ReceiverStateMachine(unsigned char buf, int* state_){

    switch (*state_)
    {
    case 0:
        if (buf == 0x7E)
            *state_ = 1;
        break;
    case 1:
        if (buf == 0x03)
            *state_ = 2;
        else if (buf != 0x7E)
            *state_ = 0;
        else
            *state_ = 1;
        break;
    case 2:
        if (buf == 0x85 || buf == 0x05)
            *state_ = 3;
        else if (buf == 0x01|| buf == 0x81)
            *state_ = 6;
        else if (buf != 0x7E)
            *state_ = 0;
        else
            *state_ = 1;
        break;
    case 3:
        if(buf == 0x03^0x85 || buf == 0x03^0x05){
            *state_ = 4;
        }
        else if(buf == 0x03^0x81 || buf == 0x03^0x01){
            *state_ = 7;

        }
        else if(buf != 0x7E)
            *state_ = 0;
        else
            *state_ = 1;
        break;
    case 4: 
        if(buf == 0x7E)
            *state_ = 5;
        else
            *state_ = 1;
        break;
    }


}


void changeOpenState(unsigned char buf, int* state){

    switch(*state) {
        case 0:
            if(buf == 0x7E) {
                *state = 1;
            }
            break;
        case 1:
            if(buf == 0x03) {
                *state = 2;
            } else if(buf != 0x7E) {
                *state = 0;
            } else *state = 1;
            break;
        case 2:
            if(buf == 0x07) {
                *state = 3;
            } else if(buf != 0x7E) {
                *state = 0;
            } else *state = 1;
            break;
        case 3:
            if(buf == 0x07 ^ 0x03) {
                *state = 4;
            } else if(buf != 0x7E) {
                *state = 0;
            } else *state = 1;
            break;
        case 4:
            if(buf == 0x7E) {
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

int llopen(int fd) {
    int state = 0;
    unsigned char set[BUF_SIZE];
    unsigned char ua[BUF_SIZE];
    set[0] = 0x7E;
    set[1] = 0x03;
    set[2] = 0x03;
    set[3] = 0x03 ^ 0x03;
    set[4] = 0x7E;

    ua[0] = 0x7E;
    ua[1] = 0x03;
    ua[2] = 0x07;
    ua[3] = 0x03 ^ 0x07;
    ua[4] = 0x7E;  

    // Create string to send
    unsigned char buf[BUF_SIZE] = {0};

    (void)signal(SIGALRM, alarmHandler);
    


    while (alarmCount < 3) {
        
        if (alarmEnabled == FALSE) {
            write(fd, set, BUF_SIZE);
            alarmEnabled = TRUE;
            alarm(3); // Set alarm to be triggered in 3s
            state = 0;
        }
        if (alarmCount == 3)
            break;

        read(fd, buf, 1);

        //printf("var = 0x%02X state:%d\n", (unsigned int)(buf[0] & 0xFF), state);
        changeOpenState(buf[0], &state);

        if (state == 5) {
            alarm(0);
            break;
        }
    }
    printf("llopen  - write\n");
    return 0;
}

void changeCloseState(unsigned char buf, int* state) {

    switch(*state) {
        case 0:
            if(buf == 0x7E) {
                *state = 1;
            }
            break;
        case 1:
            if(buf == 0x01) {
                *state = 2;
            } else if(buf != 0x7E) {
                *state = 0;
            } else *state = 1;
            break;
        case 2:
            if(buf == 0x0B) {
                *state = 3;
            } else if(buf != 0x7E) {
                *state = 0;
            } else *state = 1;
            break;
        case 3:
            if(buf == 0x01 ^ 0x0B) {
                *state = 4;
            } else if(buf != 0x7E) {
                *state = 0;
            } else *state = 1;
            break;
        case 4:
            if(buf == 0x7E) {
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

int llclose(int fd) {
 int state = 0;
    unsigned char disc[BUF_SIZE];
    unsigned char ua[BUF_SIZE];
    disc[0] = 0x7E;
    disc[1] = 0x03;
    disc[2] = 0x0B;
    disc[3] = 0x03 ^ 0x0B;
    disc[4] = 0x7E;


    ua[0] = 0x7E;
    ua[1] = 0x01;
    ua[2] = 0x07;
    ua[3] = 0x01 ^ 0x07;
    ua[4] = 0x7E;  

    // Create string to send
    unsigned char buf[BUF_SIZE] = {0};

    (void)signal(SIGALRM, alarmHandler);
    


    while (alarmCount < 3) {
        
        if (alarmEnabled == FALSE) {
            write(fd, disc, BUF_SIZE);
            alarmEnabled = TRUE;
            alarm(3); // Set alarm to be triggered in 3s
            state = 0;
        }
        if (alarmCount == 3)
            break;

        read(fd, buf, 1);

        //printf("var = 0x%02X state:%d\n", (unsigned int)(buf[0] & 0xFF), state);
        changeCloseState(buf[0], &state);

        if (state == 5) {
            alarm(0);
            write(fd, ua, BUF_SIZE);
            //printf("Sent UA\n");
            break;
        }
    }
    printf("llclose - write\n");
    return 0;

}

int specialByteCount(unsigned char* data, int size) {
    int total = 0;
    for(int i = 0; i < size; i++)
        if (data[i] == 0x7E | data[i] == 0x7D)
            total++;
    return total;
}

int stuffArray(unsigned char* data,unsigned char* res, int size) {
    int j = 0, i = 0;
    for (i = 0; i < size; i++) {
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


int llwrite(int fd, unsigned char* data, int N, int frame_index) {


    // F  | A | N(s) | BCC1 | Dados + B| F =  I

    // XOR de tudo -> BCC2 implica um array com tamanho size + 1

    // Stuffing  0x7E -> 7D 5E
    //          0x7D -> 7D 5D    // Definir  N(s) 0/1   ->  0x00 / 0x40 iniciar var a 0 ir alterando
    // Inserir F's e A

    // Build Payload
    unsigned char newData[N + 1];
    calc_BBC_2(data,newData,N);
    data = newData;

    int payload_size = N + 1 + specialByteCount(data, N + 1);
    unsigned char payload[payload_size];
    int frame_size = 5 + payload_size;
    unsigned char frame[frame_size];

    if (payload_size != N + 1)          // eficiencia
        stuffArray(data, payload, N + 1);




    frame[0] = 0x7E;                // First Flag
    frame[1] = 0x03;                // A
    frame[2] = frame_index * 0x40;  // N(s)
    frame[3] = frame[1] ^ frame[2]; // BCC1 
    for(int i = 0; i < payload_size; i++) { // payload
        frame[4 + i] = payload[i];
    }
    frame[frame_size - 1] = 0x7E;   // Final Flag

    alarmCount = 0;
    alarmEnabled = FALSE;
    (void)signal(SIGALRM, alarmHandler);

    while(alarmCount < 3){ 
    
    if(alarmEnabled == FALSE){ 
        printf("Sending frame\n");
        write(fd, frame, frame_size);
        alarmEnabled = TRUE;
        alarm(3);
        }

    unsigned char buf[BUF_SIZE] = {0};
    unsigned char buffer[1];
    int state_ = 0;
   
    while(state_ != 5 && alarmEnabled == TRUE){

        read(fd,buffer,1);
        ReceiverStateMachine(buffer[0],&state_);
        if(state_ > 0 && state_ <= 5){
            buf[state_ - 1] = buffer[0];
        }
        else if (state_ == 6){
            printf("Recebeu REJ\n");
            printf("Erro\n");
            state_ = 0;
        }
        else if (state_ == 7){
            printf("Recebeu BBC1 errado\n");
            printf("Erro\n");   
            return -1;
        }
       // printf("state: %d\n",state_);
       
        //printf("x0%02X-",buffer[0]);
    }
    if(state_ == 5){
        //printf("Recebeu RR\n");
        printf("llwrite\n");
        alarm(0);
        return 0;
    }
    
    printf("No cena found\n");
   
    //for (int i = 0; i < BUF_SIZE; i++)
        //printf("0x%02X ", buf[i]);
    //printf("\n");

    }

    return 0;


    }

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
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

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 1; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the pvalue of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");
    unsigned char t[10];
    for (int i= 0; i < 10; i++) t[i] = 0xFF; t[2] = 0x7D; t[9] = 0x7E;
    llopen(fd);
    
    llwrite(fd,t, 10, 1);

    llclose(fd);

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
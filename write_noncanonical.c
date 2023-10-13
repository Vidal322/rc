#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

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
        j++;
    }
    return 0;
}

int llwrite(int fd, unsigned char* data, int N) {
    // F  | A | N(s) | BCC1 | Dados | F =  I

    // XOR de tudo -> BCC2 implica um array com tamanho size + 1

    // Stuffing  0x7E -> 7D 5E
    //          0x7D -> 7D 5D
    // Definir  N(s) 0/1   ->  0x00 / 0x40 iniciar var a 0 ir alterando
    // Inserir F's e A

    int payload_size = N + specialByteCount(data, N);
    unsigned char payload[payload_size];

    if (payload_size != N)
        stuffArray(data, payload,N);

    for (int i = 0; i < N; i++)
        printf("0x%02X ", data[i]);
    printf("\n");
    for (int i = 0; i < payload_size; i++)
        printf("0x%02X ", payload[i]);
    printf("\n");
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
    //llopen(fd);

    llwrite(fd,t, 10);

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
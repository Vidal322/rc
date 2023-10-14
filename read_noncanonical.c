// Read from serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

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

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256

volatile int STOP = FALSE;


int llopen(int fd) {
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
    ua[3] = 0x07 ^ 0x03;
    ua[4] = 0x7E;  


    unsigned char buf[BUF_SIZE] = {0}; 

    int state = 0;

    while (STOP == FALSE)
    {

        if (read(fd, buf, 1) == 0)
            continue;


        if (buf[0] == set[state])
            state++;
        else
            state = 0;

        printf("var = 0x%02X  state = %d \n", (unsigned int)(buf[0] & 0xFF), state);



        if (state == 5)
            STOP = TRUE;
    }
    write(fd, ua, BUF_SIZE);
    printf("sent acknowledge package\n");

}
/*
int close(int fd) {
     int state = 0;
    unsigned char ua[BUF_SIZE];
    unsigned char disc[BUF_SIZE];

    ua[0] = 0x7E;
    ua[1] = 0x03;
    ua[2] = 0x07;
    ua[3] = 0x03 ^ 0x07;
    ua[4] = 0x7E;  

    disc[0] = 0x7E;
    disc[1] = 0x03;
    disc[2] = 0x0B;
    disc[3] = 0x03 ^ 0x0B;
    disc[4] = 0x7E;  

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
        changeDiscState(buf[0], &state);

        if (state == 5) {
            alarm(0);
            break;
        }
    }
    return 0;
}*/
 
int check_BBC_2 (unsigned char* data,unsigned BBC, int size){
    unsigned tmp = data[0];
    for(int i = 1; i<size ; i++){
        tmp ^= data[i];
    }
    return tmp == BBC;
}

//int check_BBC ()

int untsuffArray(unsigned char* array) {

}

int send_RR (int fd,unsigned char frame_index, int success){
    unsigned char tmp = frame_index == 0 ? 0x85 : 0x05;
    unsigned char tmp2 = frame_index == 0 ? 0x01 : 0x81; //MUDAR DE NOVO
    
    unsigned char RR[5];

    RR[0] = 0x7E;
    RR[1] = 0x03;
    RR[2] = success ?  tmp : tmp2;
    RR[3] = RR[1] ^ RR[2];
    RR[4] = 0x7E;

    write(fd,RR,5);
}


void changeReadState(unsigned char buf, int* state, int* index,unsigned char* frame_index){

    switch(*state) { // tem que retornar os RR e as outras ceans do genero
        case 0:
            if(buf == 0x7E) {
                *state = 1;
            }
            break;
        case 1:
            if(buf == 0x03) {
                *state = 2;
            } 
            else if(buf != 0x7E) {
                *state = 0;
            } 
            else *state = 1;
            break;
        case 2:
            if(buf == 0x00) {
                *state = 3;
                *frame_index = 0;}
            
                else if (buf == 0x40){
                    *state = 3;
                    *frame_index = 1;
                } 
             else if(buf != 0x7E) {
                *state = 0;
            } else *state = 1;
            break;
        case 3:
            if(buf == 0x43 || buf == 0x03) { // meter a enviar o RRJ
                *state = 4;
            } else if(buf != 0x7E) {
                *state = 0;
            } else *state = 1;
            break;
        case 4:
            if(buf == 0x7E) {
                *state = 1;
            } else {
                *state = 5;
            }
            break;
        case 5: 
            if(buf == 0x7E)
                *state = 6;
             // nao esquecer aquele byte no final temos de mudar isto
            break; 
 

        default:
            *state = 0;
            break;
    }

    if (*state > 0) (*index)++;
}


int llread(int fd, unsigned char* result) {
    unsigned char buf[1];
    int state = 0, index = -1;
    int size;
    unsigned char frame_index;

    while(state != 6) {
        read(fd, buf, 1);
        printf("buf: 0x%02X \n",buf[0]);
        changeReadState(buf[0], &state, &index, &frame_index);
        printf("INDEX:%d  STATE: %d  SIZE: %d FRAME: %d\n",index,state,size,frame_index);
        if (state > 0)
            result[index] = buf[0];
    }
    size = index + 1;
     for (int i = 0; i < size; i++)
        printf("0x%02X ", result[i]);
    printf("\n");

    send_RR(fd,frame_index,1); // temos de ver como é suposto saber se vai bem ou nao, acho q tem a ver com os bcc e essas cenas mas n tenho a certeza
    
    
    //

}

// calcula o exor de tudo o que recebeu -> coincide? boa
// recebeu N(s) = 0 manda reader_ready 1 vice versa -> variavel local inicia a 0


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

    // Open serial port device for reading and writing and not as controlling tty
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
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }


    printf("New termios structure set\n");

    llopen(fd);
    unsigned char result[20];

    llread(fd,result);
















    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}

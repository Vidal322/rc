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
            if(buf == 0x03) {
                *state = 3;
            } else if(buf != 0x7E) {
                *state = 0;
            } else *state = 1;
            break;
        case 3:
            if(buf == 0x03 ^ 0x03) {
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

    unsigned char ua[BUF_SIZE];

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

        changeOpenState(buf[0], &state);

        printf("var = 0x%02X  state = %d \n", (unsigned int)(buf[0] & 0xFF), state);

        if (state == 5)
            STOP = TRUE;
    }
    write(fd, ua, BUF_SIZE);
    printf("sent acknowledge package\n");
}

void close_Read_UA(unsigned char buf, int* state){

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
            if(buf == 0x07) {
                *state = 3;
            } else if(buf != 0x7E) {
                *state = 0;
            } else *state = 1;
            break;
        case 3:
            if(buf == 0x07 ^ 0x01) {
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

void changeCloseState(unsigned char buf, int* state){

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
            if(buf == 0x0B) {
                *state = 3;
            } else if(buf != 0x7E) {
                *state = 0;
            } else *state = 1;
            break;
        case 3:
            if(buf == 0x03 ^ 0x0B) {
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

int llclose(int fd){
    unsigned char disc[BUF_SIZE];
    unsigned char ua[BUF_SIZE];

    disc[0] = 0x7E;
    disc[1] = 0x01;
    disc[2] = 0x0B;
    disc[3] = 0x0B ^ 0x01;
    disc[4] = 0x7E;  

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

        changeCloseState(buf[0], &state);

        printf("var = 0x%02X  state = %d \n", (unsigned int)(buf[0] & 0xFF), state);

        if (state == 5)
            STOP = TRUE;
    }
    write(fd, disc, BUF_SIZE);
    printf("sent discacknoledge package\n");
    STOP = FALSE;
    while (STOP == FALSE)
    {

        if (read(fd, buf, 1) == 0)
            continue;

        close_Read_UA(buf[0], &state);

        printf("var = 0x%02X  state = %d \n", (unsigned int)(buf[0] & 0xFF), state);

        if (state == 5){
            STOP = TRUE; 
            printf("received UA\n");
        }
           

    }

    return 0;

    

}

 
int check_BBC_2 (unsigned char* data,unsigned BBC, int size){
    unsigned tmp = data[0];
    for(int i = 1; i<size ; i++){
        tmp ^= data[i];
    }
    return tmp == BBC;
}


int unstuffArray(unsigned char* data, unsigned char* res, int size) {
    int j = 0, i = 0;
    for (i = 4; i < size-1; i++) {
        if (data[i] == 0x7D && i + 1 < size && (data[i+1] == 0x5E || data[i+1] == 0x5D)) {
            if (data[i+1] == 0x5E)
                res[j] = 0x7E;
            else
                res[j] = 0x7D;
            i++; 
        }
        else
            res[j] = data[i];
        j++;
    }
    return 0;
}

int send_RR (unsigned char frame_index){
    
    unsigned char RR[5];

    RR[0] = FLAG;
    RR[1] = A_tx;
    RR[2] = C_RR1 * frame_index;
    RR[3] = RR[1] ^ RR[2];
    RR[4] = FLAG;

    write(fd,RR,5);
}

int send_REJ (unsigned char frame_index){

    unsigned char RR[5];

    RR[0] = FLAG;
    RR[1] = A_tx;
    RR[2] = C_REJ1 * frame_index;
    RR[3] = RR[1] ^ RR[2];
    RR[4] = FLAG;

    write(fd,RR,5);
}



void changeReadState(unsigned char buf, int* state, int* index){

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
                *index = 0;
            } 
            else { 
                *state = 1;
                *index = 0;
            }
            break;
        case 2:
            if(buf == 0x00 || buf == 0x40) {
                *state = 3;}
            
             else if(buf != 0x7E) {
                *state = 0;
                *index = 0;

            } else {
                *state = 1;
                *index = 0;
                }
            break;
        case 3:
            if(buf == 0x43 || buf == 0x03) {
                *state = 4;
            } else if(buf != 0x7E) {
                *state = 0;
                *index = 0;
            } else {
                *state = 1;
                *index = 0; 
                }
            break;
        case 4:
            if(buf == 0x7E) {
                *state = 1;
                *index = 0;
            } else {
                *state = 5;
            }
            break;
        case 5: 
            if(buf == 0x7E)
                *state = 6;
            break; 
 

        default:
            *state = 0;
            break;
    }
}

int validate_header(unsigned char* data, int* valid_header, int* duplicate, int frame_index) {        //retorna 1 se for válido
    valid_header = 1;
    if (data[1] ^ data[2] != data[3]) {
        valid_header == 0;
        return 0;
    }
    duplicate = 0;
    if (data[2] != frame_index){
        duplicate = 1;             
        return 0;
    }

    return 0;
}
int validate_data(unsigned char* res, int* valid_data, int size){

    unsigned char bcc2 = res[size-1];
    unsigned char res_value = res[0];

    for (int i = 5 ; i < size-2 ; i++){

        res_value ^= res[i];
    } 

    if(res_value != bcc2)
        valid_data = 0;


}

int packet_validation(int fd, unsigned char* frame_index,unsigned char* data,unsigned char* res,int size){ // 1 - mandar packet para aplicação
    //validar o header
    int valid_header,duplicate, valid_data;
    validate_header(data,&valid_header,&duplicate,frame_index);
    unstuffArray(data,res,size);
    validate_data(res,&valid_data,size);

    if(valid_header == 0){
        printf("invalid header\n");
        return 0;
    }

    if(valid_data == 1){
        send_RR(fd,frame_index);

        if(duplicate == 1){
            printf("duplicate\n");
            return 0; 
            }

        else {
            printf("not duplicate\n");
            *frame_index++;
            *frame_index %= 2;
            return 1;
        }
     
    }         

    else {
        if(duplicate == 1){
            send_RR(fd,frame_index);
            printf("duplicate\n");
            return 0; 
            }
        else {
            send_REJ(fd,frame_index);
            printf("Not Duplicate\n");
            return 0;
        }

    }       

    }



int llread(int fd, unsigned char* result,unsigned char* frame_index) {
    int size = 20;
    unsigned char* received_packet[size];
    int state = 0, index = 0;
    unsigned char buf[1];


    while(state != 6) {
        read(fd, buf, 1);
        //printf("buf: 0x%02X \n",buf[0]);
        changeReadState(buf[0], &state, &index);
        //printf("INDEX:%d  STATE: %d  SIZE: %d FRAME: %d\n",index,state,size,frame_index);
        if (state > 0)
            received_packet[index++] = buf[0];
    }
  //  size = index;  // fazer uma função que consoante o size da trama deve aumentar o tamanho do result
    /* for (int i = 0; i < size; i++)
        printf("0x%02X ", result[i]);
    printf("\n");*/

    
   
    packet_validation(fd,frame_index,received_packet,result,size);// temos de ver como é suposto saber se vai bem ou nao, acho q tem a ver com os bcc e essas cenas mas n tenho a certeza
    
    
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
    // tcflush() discahttps://go.codetogether.com/#/2dd26d73-91f3-44f6-811b-d42fa3965a51/8lIB97rWTDTEhZmQPQ0ogLe value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }


    printf("New termios structure set\n");


    unsigned char frame_index = 0;

    unsigned char t[10];
    unsigned char res[10];
    for (int i= 0; i < 10; i++) t[i] = 0xFF; t[6] = 0x7D; t[7] = 0x5D;

    unstuffArray(t,res,10);
    for(int i = 0 ; i < 5; i++)
    printf("0x%02X ",res[i]);
    printf("\n");
    for(int i = 5 ; i < 10; i++)
    printf("0x%02X ",t[i]);

    /*llopen(fd);
    unsigned char result[20]; // ele tem de aumentar consoante o tamanho do pacote

    llread(fd,result, &frame_index);

    llclose(fd);*/








    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
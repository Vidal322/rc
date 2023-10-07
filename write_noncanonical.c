// Write to serial port in non-canonical mode
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

#define BUF_SIZE 5

volatile int STOP = FALSE;


#define FALSE 0
#define TRUE 1

int alarmEnabled = FALSE;
int alarmCount = 0;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
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
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 1;  // Blocking read until 5 chars received

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

    // Create string to send
    unsigned char buf[BUF_SIZE] = {0};

    int state = 0;

    (void)signal(SIGALRM, alarmHandler);

    while (alarmCount < 3) {
        
        if (alarmEnabled == FALSE) {
            alarmEnabled = TRUE;
            alarm(2); // Set alarm to be triggered in 3s
            state = 0;
            write(fd, set, BUF_SIZE);
        }
        read(fd, buf, 1);

        printf("var = 0x%02X state:%d\n", (unsigned int)(buf[0] & 0xFF), state);

        if (buf[0] == ua[state])
            state ++;
        else
            state = 0;

        if (state == 5) {
            alarm(0);
            break;
        }
    }
    if (alarmCount == 3)
        printf("No response. Ending program");

    else
        printf("Sucess. Ending program\n");



    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}

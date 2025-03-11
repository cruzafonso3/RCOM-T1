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

int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal)   // User-defined function to handle alarms (handler function)
{          
                      // This function will be called when the alarm is triggered
alarmEnabled = TRUE;           // Can be used to change a flag that increases the number of alarms

}

int main(int argc, char *argv[])
{
    (void) signal(SIGALRM, alarmHandler);
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

    fcntl (fd, F_SETFL, O_NONBLOCK);

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
    newtio.c_cc[VMIN] = 5;  // Blocking read until 5 chars received

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

    // Create string to send
    unsigned char buf[BUF_SIZE] = {0};

    /*for (int i = 0; i < BUF_SIZE; i++)
    {
        buf[i] = 'a' + i % 26;
    }*/


    buf[0]= 0x7E;
    buf[1]= 0x03;
    buf[2]= 0x03;
    buf[3]= 0x03 ^ 0x03;
    buf[4]= 0x7E;
    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.
   

    int bytes = write(fd, buf, BUF_SIZE);
    //int bytes = 0;
    printf("%d bytes written\n", bytes);

//------------------------------------------------------------------------
// -------------Maquina de estados implementada na aula 2-----------------   
//------------------------------------------------------------------------

int estado = 0;
unsigned char current;
alarm(3);

while(1) 
{
    
    if (alarmCount == 4 && alarmEnabled == TRUE){
        alarm (0);
        break;
    }

    if (alarmEnabled == TRUE && alarmCount < 4)
    {
        alarmCount++;
        printf("Alarm #%d\n", alarmCount);
        printf("RESENDING__\n"); 
        alarmEnabled = FALSE;
        bytes = 0;
        write(fd, buf, BUF_SIZE);
        printf("%d bytes written\n\n", bytes);
        alarm(3); 
        estado = 0;
        continue;
    }
    if (alarmEnabled == FALSE){
        if(read(fd, &current, 1) == -1){continue;}
    }

    



    switch (estado)
    {
        case(0):
            if(current==0x7E) estado = 1;

            printf("Start\t\n");    
        break;

        case(1):
            if(current == 0x7E) estado = 1;
            else if(current == 0x01) estado = 2;
            else estado = 0;
            
            printf("FLAG_RCV\t\n");
        break;

        case (2):
            if(current == 0x7E) estado = 1;
            else if(current == 0x07) estado = 3;
            else estado = 0;
        
            printf("A RCV\t\n");
        break;

        case (3):
            if(current == 0x7E) estado = 1;
            else if(current == 0x01^0x07) estado = 4;
            else  estado = 0;
        
            printf("C RCV\t\n");
        break;

        case (4):
            if(current == 0x7E) estado = 5;
            else estado = 0;

            printf("BCC OK\t\n");
        break;

        case (5):
            

        break;
    }

    printf("0x%02X   ->  ", current);
    if(estado == 5) 
    {
        printf("STOP\n\n");
        break;
        alarm(0);
    }
    
 
}


//-----------------------------------------------------------------------------
// -------------Fim da maquina de estados implementada na aula 2---------------
//-----------------------------------------------------------------------------
    





//-----------------------------------------------------------------------------
// -----------------------------------AULA 3-----------------------------------  
//-----------------------------------------------------------------------------

unsigned char buff[10] = {0x7E,0x03,0x03,0x03 ^ 0x03,'D','R','I','P','D'^'R'^'I'^'P',0x7E};
   
int bytes1 = write(fd, buff, 10);
printf("NEW FSM\n\t%d bytes written\n", bytes1);

//-----------------------------------------------------------------------------
// -----------------------------------AULA 3-----------------------------------  
//-----------------------------------------------------------------------------


    // Wait until all bytes have been written to the serial port
    //sleep(1);

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}

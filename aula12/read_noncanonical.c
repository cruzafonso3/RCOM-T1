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

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 5

volatile int STOP = FALSE;

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

    printf("New termios structure set\n\n");

    // Loop for input
    unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char


//------------------------------------------------------------------------
// -------------Maquina de estados implementada na aula 2-----------------   
//------------------------------------------------------------------------

int estado = 0;
unsigned char current = 0;

while(1)
{
    read(fd, &current, 1);

    switch (estado)
    {
        case(0):
            if(current==0x7E) estado = 1;

            printf("Start\t\n");    
        break;

        case(1):
            if(current == 0x7E) estado = 1;
            else if(current == 0x03) estado = 2;
            else estado = 0;
            
            printf("FLAG_RCV\t\n");
        break;

        case (2):
            if(current == 0x7E) estado = 1;
            else if(current == 0x03) estado = 3;
            else estado = 0;
        
            printf("A RCV\t\n");
        break;

        case (3):
            if(current == 0x7E) estado = 1;
            else if(current == 0x00) estado = 4;
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
    if(estado == 5){
        printf("STOP\n");
        break;
    }
}   

//-----------------------------------------------------------------------------
// -------------Fim da maquina de estados implementada na aula 2---------------
//-----------------------------------------------------------------------------
        
    //Create string to send
   
    buf[0]=0x7E;
    buf[1]=0x01;
    buf[2]=0x07;
    buf[3]=0x01^0x07;
    buf[4]=0x7E;
    


    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.
    //buf[5] = '\n';

    int bytes = write(fd, buf, BUF_SIZE);
    printf("\n%d bytes written\n\n", bytes);


//-----------------------------------------------------------------------------
// -----------------------------------AULA 3-----------------------------------  
//-----------------------------------------------------------------------------
    int pointer =0;
    estado = 0;
    


    unsigned char new[5];
    while(1)
    {
        read(fd, &current, 1);
    
        switch (estado)
        {
            case(0):
                if(current==0x7E) estado = 1;
    
                printf("Start\t\n");    
            break;
    
            case(1):
                if(current == 0x7E) estado = 1;
                else if(current == 0x03) estado = 2;
                else estado = 0;
                
                printf("FLAG_RCV\t\n");
            break;
    
            case (2):
                if(current == 0x7E) estado = 1;
                else if(current == 0x03) estado = 3;
                else estado = 0;
            
                printf("A RCV\t\n");
            break;
    
            case (3):
                if(current == 0x7E) estado = 1;
                else if(current == 0x00) estado = 4;
                else  estado = 0;
            
                printf("C RCV\t\n");
            break;
    
    
            case (4):
                if(current!=0x7E){
                    new[pointer]= current;
                    pointer ++;

                }
                else if(current==0x7E){
                    if(new[5]==new[1]^new[2]^new[3]^new[4])
                {
                    printf("BCC2\t\n");
                    estado=5;
                }
                else  printf("BCC2 NOT OK\t\n");
                }
                
            break;
        }
    
        printf("0x%02X   ->  ", current);
        if(estado == 5){
            printf("MESSAGE SENT:");
                    for (int i = 0; i<5;i++) {printf("%c",new[i]);}
                    printf("\n");
            printf("STOP\n");
            break;
        }
    }  






//-----------------------------------------------------------------------------
// -----------------------------------AULA 3-----------------------------------  
//-----------------------------------------------------------------------------






    // Wait until all bytes have been written to the serial port
    sleep(1);

    //for (int i=0;i<5;i++) {printf(buf[i]);}
    // The while() cycle should be changed in order to respect the specifications
    // of the protocol indicated in the Lab guide

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}

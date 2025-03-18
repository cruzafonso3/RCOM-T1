// Link layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <termios.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/select.h>

// MISC
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source
#define FALSE 0
#define TRUE 1
#define BUF_SIZE 5
#define MAX_FRAME_SIZE 256

volatile int STOP = FALSE;
int alarmEnabled = FALSE;
int alarmCount = 0;
int ab = 0;
int fd = 0;
int identifier = 0;

// Copied from the code given by professors
int startconnection(const char *serialPort) {

    fd = open(serialPort, O_RDWR | O_NOCTTY);
    fcntl(fd, F_SETFL, O_NONBLOCK);
    if (fd < 0) {
        perror(serialPort);
        return -1; 
    }

    // Save current port settings
    struct termios oldtio;
    struct termios newtio;

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
    newtio.c_cc[VMIN] = 0;  

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.

    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        return -1;
    }
    
    return fd;
}

void alarmHandler(int signal) {
    alarmEnabled = TRUE;    
}       

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{   
    printf("\n\n---------------------------STARTING LLOPEN:---------------------------\n\n");
    int fd = startconnection(connectionParameters.serialPort);
    if (fd < 0) return -1;
    unsigned char buf[5] = {0};
    unsigned char buf1[5] = {0};

    // TX
    if(connectionParameters.role == LlTx) {
        (void) signal(SIGALRM, alarmHandler);
        printf("New termios structure set\n");
    
        buf[0] = 0x7E;
        buf[1] = 0x03;  
        buf[2] = 0x03;  
        buf[3] = 0x03 ^ 0x03;  
        buf[4] = 0x7E;
    
        int bytes = write(fd, buf, BUF_SIZE);
        printf("%d bytes sent\n", bytes);
    
        int estado = 0;
        unsigned char current;
        alarm(3);  
    
        while(1) {
            if (alarmEnabled) {
                alarmCount++;
                printf("Alarm #%d\n", alarmCount);
    
                if (alarmCount >= 4) {
                    printf("Maximum attempts reached. Aborting.\n");
                    break;
                }
                printf("RESENDING...\n\n");
                write(fd, buf, BUF_SIZE);

                alarmEnabled = FALSE;
                alarm(3);
            }
    
            
            if (read(fd, &current, 1) > 0) {
                
    
                switch (estado) {
                    case 0:  // START
                        if (current == 0x7E) estado = 1;
                        printf("Start SET\t\n");    
                    break;
    
                    case 1:  // FLAG_RCV
                        if (current == 0x01) estado = 2;
                        else if (current != 0x7E) estado = 0;
                        printf("FLAG_RCV\t\n");

                    break;
    
                    case 2:  // A_RCV
                        if (current == 0x07) estado = 3;
                        else if (current == 0x7E) estado = 1;
                        else estado = 0;
                        printf("A RCV\t\n");
                    break;
    
                    case 3:  // C_RCV
                        if (current == 0x06) estado = 4;
                        else estado = 0;
                        printf("C RCV\t\n");
                    break;
    
                    case 4:  // BCC1_OK
                        if (current == 0x7E) {
                            alarm(0);  // Cancela o alarme
                            printf("BCC OK\t\n");
                            estado = 5;
                        }
                    break;
                }

                printf("0x%02X   ->  ", current); 
                if (estado == 5) {
                    printf("STOP\n");
                    break;
                }
            }  
        }
    }    


    // RX
    else if(connectionParameters.role==LlRx){
        int estado = 0;
        unsigned char current = 0;

        struct timeval timeout;
        timeout.tv_sec = 60;  // Timeout de 60 segundos
        timeout.tv_usec = 0;

        while(1)
        {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(fd, &read_fds);

            int ret = select(fd + 1, &read_fds, NULL, NULL, &timeout);
            if (ret == 0) {
            printf("Timeout: No data received\n");
            break;
            }   else if (ret == -1) {
                perror("select");
                exit(-1);
            }


            ab = read(fd, &current, 1);
            if (ab == -1) continue;

            switch (estado)
            {
                case 0: // START
                    if(current==0x7E) estado = 1;
                    printf("Start UA\t\n");    
                break;

                case 1: // FLAG_RCV
                    if(current == 0x7E) estado = 1;
                    else if(current == 0x03) estado = 2;
                    else estado = 0;
                    printf("FLAG_RCV\t\n");
                break;

                case 2: //A_RCV
                    if(current == 0x7E) estado = 1;
                    else if(current == 0x03) estado = 3;
                    else estado = 0;
                    printf("A RCV\t\n");
                break;

                case 3: //C_RCV
                    if(current == 0x7E) estado = 1;
                    else if(current == 0x00) estado = 4;
                    else  estado = 0;
                    printf("C RCV\t\n");
                break;

                case 4: // BCC1_OK
                if(current == 0x7E) estado = 5;
                else estado = 0;
                printf("BCC OK\t\n");
                break;

                case 5: // STOP
                break;
            }

            printf("0x%02X   ->  ", current); 
            if(estado == 5){ 
                printf("STOP\n"); 
                break;
            }
        }   

        buf1[0]=0x7E;
        buf1[1]=0x01;
        buf1[2]=0x07;
        buf1[3]=0x01^0x07;
        buf1[4]=0x7E;
        int bytes = write(fd, buf1, BUF_SIZE);
        printf("\n%d bytes sent\n", bytes);

    }  

    return fd;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize) {

    int packet_size = bufSize + 100;
    unsigned char packet[packet_size];

    // Supervision frame
    packet[0] = 0x7E; // FLAG
    packet[1] = 0x03; // A
    if(identifier == 0) {
        packet[2] = 0x00; // C - I (0)
        identifier = 1;
    } 
    else {
        packet[2] = 0x40; // C - I (1)
        identifier = 0;
    }   
    packet[3] = packet[2] ^ packet[1]; // BCC1
    for (int i = 0; i < bufSize; i++) {
        for(int j = 0; j < packet_size; j++) {
            if (buf[i] == 0x7E) {
                packet[4 + j] = 0x7D; // ESCAPE
                packet[4 + j + 1] = 0x5E; 
                j++;
            }    
            else if (buf[i] == 0x7D) {
                packet[4 + j] = 0x7D; // ESCAPE
                packet[4 + j + 1] = 0x5D; 
                j++;
            } 

            else packet[4 + j] = buf[i];
        }
    }
    packet[4 + bufSize] = 0x7E; // FLAG

    // DATA FRAME READY TO BE SENT




    return 0;
}








////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO
    /*
    unsigned char buf[MAX_FRAME_SIZE];
    int estado = 0;
    int bytesRead = 0;
    static int expectedNs = 0;
    int destuffedBytes = 0;
    unsigned char destuffedBuf[MAX_FRAME_SIZE];

    while (bytesRead < MAX_FRAME_SIZE) {
        unsigned char current = packet[bytesRead++];

        switch (estado) {
            case 0:  // START
                if (current == 0x7E) estado = 1;
                break;
            case 1:  // FLAG_RCV
                if (current == 0x03) estado = 2;
                else if (current != 0x7E) estado = 0;
                break;
            case 2:  // A_RCV
                if (current == 0x00 || current == 0x40) { // I frame N(s) = 0 or 1
                    estado = 3;
                    expectedNs = (current == 0x00) ? 0 : 1;
                } else if (current == 0x7E) {
                    estado = 1;
                } else {
                    estado = 0;
                }
                break;
case 3:  // C_RCV
                if (current == (0x03 ^ packet[2])) { // Check BCC1
                    estado = 4;
                } else {
                    estado = 0;
                }
                break;
            case 4:  // DATA_RECEPTION
                if (current == 0x7E) { // End of frame
                    estado = 5;
                } else {
                    buf[destuffedBytes++] = current;
                }
                break;
        }

        if (estado == 5) { // FRAME_COMPLETE
            // Byte destuffing
            int finalBytes = 0;
            for (int i = 0; i < destuffedBytes - 1; i++) {
                if (buf[i] == 0x7D) { // Escape character
                    if (buf[i + 1] == 0x5E) {
                        destuffedBuf[finalBytes++] = 0x7E;
                    } else if (buf[i + 1] == 0x5D) {
                        destuffedBuf[finalBytes++] = 0x7D;
                    }
                    i++; // Skip next byte as it was part of escape sequence
                } else {
                    destuffedBuf[finalBytes++] = buf[i];
                }
            }

            // Compute BCC2
            unsigned char bcc2 = 0;
            for (int i = 0; i < finalBytes - 1; i++) {
                bcc2 ^= destuffedBuf[i];
            }

            if (bcc2 == destuffedBuf[finalBytes - 1]) { // BCC2 is correct
                unsigned char rrFrame[5] = {0x7E, 0x01, (expectedNs == 0) ? 0x05 : 0x85, 0x01 ^ ((expectedNs == 0) ? 0x05 : 0x85), 0x7E};
                write(fd, rrFrame, 5); // Send RR (Receiver Ready)
                memcpy(packet, destuffedBuf, finalBytes - 1);
                return finalBytes - 1;
            } else { // BCC2 error
                write(fd, "\x7E\x01\x01\x00\x7E", 5); // Send REJ (Reject)
                return -1;
            }
        }
    }
    return -1;*/
    return 0;

}







////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    return 1;
}








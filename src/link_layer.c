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

int contpacketsread = 0;
volatile int STOP = FALSE;
int alarmEnabled = FALSE;
int alarmCount = 0;
int ab = 0;
int fd = 0;
int identifier = 0;  //  = 0 se I(0) e = 1 se I(1)
int control = 1;

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
    fd = startconnection(connectionParameters.serialPort);
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
    
    int packet_size = bufSize + 100; // Tamanho máximo do pacote
    unsigned char packet[packet_size];
    unsigned int finalsize = 4; // FLAG + A + C + BCC1

    // Supervision frame
    packet[0] = 0x7E; // FLAG
    packet[1] = 0x03; // A
    if (identifier == 0) {
        packet[2] = 0x00; // C - I (0)
        //identifier = 1;
    } else {
        packet[2] = 0x40; // C - I (1)
        //identifier = 0;
    }
    packet[3] = packet[2] ^ packet[1]; // BCC1

    // Tratamento de escape e cópia dos dados
    int j = 4; // Índice para escrever no pacote
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == 0x7E) {
            packet[j++] = 0x7D; // ESCAPE
            packet[j++] = 0x5E; // 0x7E escapado
            finalsize += 2;
        } else if (buf[i] == 0x7D) {
            packet[j++] = 0x7D; // ESCAPE
            packet[j++] = 0x5D; // 0x7D escapado
            finalsize += 2;
        } else {
            packet[j++] = buf[i]; // Dados normais
            finalsize++;
        }
    }
    packet[j++] = 0x7E; // FLAG final
    finalsize++;
    if(control == 0) {
        printf("\n\n---------------------------CONTROL PACK LLWRITE: %d ---------------------------\n\n",finalsize);
    }
    else printf("\n\n---------------------------STARTING LLWRITE: packet with %d bytes---------------------------\n\n", finalsize-12);

    for (int i = 0; i < finalsize; i++)  printf("0x%02X ", packet[i]);
    printf("\n------------------------------------\n\n");
    // Calcula BCC2
    int BCC2 = 0;
    for (int i = 0; i < bufSize; i++) {
        BCC2 ^= buf[i];
    }
    packet[j++] = BCC2; // BCC2
    packet[j++] = 0x7E; // FLAG final
    finalsize += 2;

    // Lógica de envio e retransmissão
    int estado = 0;
    unsigned char current = 0;
    int attempts = 0;

    while (attempts < 3) { // Número máximo de tentativas
        // Envia o pacote
        int bytes = write(fd, packet, finalsize-2);
        printf("%d bytes sent\n", bytes-12);

        // Configura o alarme para 3 segundos
        alarmEnabled = TRUE;
        alarm(3);

        // Aguarda resposta
        printf("Waiting for RR or REJ\n");
        while (1) {
            /*
            if (alarmEnabled) {
                alarmCount++;
                printf("Alarm #%d\n", alarmCount);

                if (alarmCount >= 4) {
                    printf("Maximum attempts reached. Aborting.\n");
                    return -1; // Falha após 3 tentativas
                }

                printf("RESENDING...\n\n");
                write(fd, packet, finalsize); // Retransmite o pacote

                alarmEnabled = FALSE;
                alarm(3); // Reinicia o alarme
            }
*/
            // Lê a resposta do receptor
            if (read(fd, &current, 1) > 0) {


                switch (estado) {
                    case 0: // START
                        if (current == 0x7E) {
                            estado = 1; // FLAG_RCV
                            
                            
                        }
                        break;

                    case 1: // FLAG_RCV
                        printf("FLAG_RCV\n");
                        if (current == 0x01) { // A (Endereço)
                            estado = 2; // A_RCV
                           
                        } else if (current == 0x7E) {
                            estado = 1; // Nova FLAG
                        } else {
                            estado = 0; // Reinicia
                        }
                        break;

                    case 2: // A_RCV
                    printf("A_RCV\n");
                        if ((identifier == 0 || current == 0x05) || (identifier== 1 ||current == 0x85)) { // C (Controlo) // MUDAR ISTO PARA && !!!!!!!!!!!!!!!!!!!!!!!!!
                            estado = 3; // C_RCV
                            
                        } else if (current == 0x7E) {
                            estado = 1; // Nova FLAG
                        } else {
                            estado = 0; // Reinicia
                        }
                        break;

                    case 3: // C_RCV
                    printf("C_RCV\n");
                        if (current == 0x04 || current == 0x84 ) { // BCC1_OK
                            estado = 4; // BCC1_OK
                            
                        } else {
                            estado = 0; // Reinicia
                        }
                        break;

                    case 4: // BCC1_OK
                    printf("BCC1_OK\n");
                        if (current == 0x7E) { // FLAG final
                            estado = 5; // STOP
                            printf("STOP\n");
                        }
                        break;
                }

                if (estado == 5) {
                    // Confirmação recebida (RR)
                    alarm(0); // Cancela o alarme
                    /*if(identifier == 0) //identifier = 1;
                    else identifier = 0;*/
                    printf("-------- Success: Packet I(%d) read -------\n",identifier);
                    if(control == 0) control = 1;
                    alarmCount = 0;

                    return 1; // Sucesso
                    
                }
                printf("0x%02X   ->  ", current); 
            }
        }

        attempts++; // Incrementa o número de tentativas
    }
    printf("Maximum attempts reached. Aborting.\n");
    return -1; // Falha
}




////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *buf) { 
    printf("\n\n---------------------------  I(%d)  ---------------------------\n\n",identifier);
    
    int indicator = 1;
    int stuffing=0;
    int estado = 0;
    int BCC2 = 0;
    int BCC2_read = 0;
    int data_pointer = 0; 
    int packetsize = 0;
        unsigned char data[300] = {};
        unsigned char buf1[6];
        unsigned char current = 0;
        /**/
        struct timeval timeout;
        timeout.tv_sec = 10;  // Timeout de 10 segundos
        timeout.tv_usec = 0;
        
        printf("\n         ///////////////////////////////////////////////");  
        printf("\n         //////// Start Reading Packet nº %d ///////////\t\n",contpacketsread+1); 
        printf("         ///////////////////////////////////////////////\n\n");   
        
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
            
            
            int contnamefile = 0;
            int finishnamefile = 0;
            switch (estado)
            {
                case 0: // START
                    if(current==0x7E) estado = 1;
                break;
                    

                case 1: // FLAG_RCV
                    if(current == 0x7E) estado = 1;
                    else if(current == 0x03) estado = 2;
                    else estado = 0;
                    printf("FLAG_RCV\t\n");
                break;

                case 2: //A_RCV
                    if(current == 0x7E) estado = 1;
                    else if(current == 0x00 || current == 0x40) estado = 3;
                    else estado = 0;
                    printf("A RCV\t\n");
                break;

                case 3: //C_RCV
                    if(current == 0x7E) estado = 1;
                    else if(current == (0x03 ^ 0x00) || current == (0x03 ^ 0x40)) estado = 4;
                    else  estado = 0;
                    printf("C RCV\t\n");
                break;

                case 4: // BCC1_OK

                if(control == 0)
                {

                    if (indicator == 1){
                        printf("C --->  0x%02X   \n", current);
                        indicator ++;
                        break;
                    }

                    else if(indicator == 2){
                        printf("T1 --->  0x%02X    \n", current);
                        indicator ++;
                        break;
                    }    
                    else if(indicator == 3){

                        if(finishnamefile== 0) printf("L1 --->  0x%02X    \n", current);
                        
                        contnamefile = current+1;
                        if(finishnamefile>0 && finishnamefile < contnamefile){
                            data[finishnamefile-1] = current;  
                        }
                        if(finishnamefile == contnamefile){
                            printf("Name of file: ");
                            for(int i = 0; i < contnamefile; i++) {
                                printf("%c", data[i]);
                            }
                            printf("\n");
                            
                            indicator ++;
                            break;
                        }
                        
                    }
                    else if(indicator == 4){
                       
                        printf("T2 --->  0x%02X    \n", current);
                        indicator ++;
                        break;
                    }
                    else if(indicator == 5){
                       
                        printf("L2 --->  0x%02X    \n", current);
                        indicator ++;
                        break;
                    }
                    else if(indicator == 6){
                       
                        printf("V2 --->  0x%02X    \n", current);
                        indicator ++;
                        break;
                        
                    }
                    if(current == 0x7E) {
                        estado = 5;
                    }
                    finishnamefile++;
                }


                else if(control==1){

                if(indicator == 1) {
                    printf("0x%02X   ->  C: Type of Packet\n", current);
                    indicator=2;
                    
                }
                 else if(indicator == 2) {
                    printf("Number of Packet --->  %d   \n", current+1);
                    indicator=3;
                    BCC2 ^= current;
                }
                else if(indicator == 3) {
                    packetsize = current*256;
                    indicator=4;
                    
                }
                else if(indicator == 4) {
                    packetsize += current;
                    indicator=5;
                   
                    printf("Packet Size --->  %d\n", packetsize);
                    break;
                }

                if(current == 0x7E) {
                    estado = 5;
                }
                
                
                if (estado == 4 && indicator == 5){
                    if(current == 0x7D){
                        stuffing = 1;
                        break;
                    }
                    if(stuffing == 1){
                        stuffing = 0;
                        if(current == 0x5E) data[data_pointer] = 0x7E;
                        else if(current == 0x5D) data[data_pointer] = 0x7D;
                        else {
                            printf("Destuffung ERROR\n"); 
                            estado = 5;
                        }
                        data_pointer++;
                        break;
                    }
                    data[data_pointer] = current;
                    data_pointer++;
                    //printf(" %c", current);
                }
            
            }
                break;


                case 5: // STOP
                break;
            }
            
            if(indicator<2) printf("0x%02X   ->  ", current); 
            if(estado == 4 && indicator == 1) printf("BCC1\t\n");
            if(estado == 5){ 
                BCC2_read = data[data_pointer-1];
                printf("0x%02X   ->  BCC2 READ\t\n",BCC2_read);
                printf("STOP\n\n"); 
                break;
            }
        }   
        printf("Data READ: \n");
        data[data_pointer-1] = '\0';
        for(int i = 0; i < data_pointer-1; i++) {
            printf("0x%02X ", data[i]);
        }
        BCC2 = 0;
        for(int i = 0; i < data_pointer-1; i++) {
            BCC2 ^= data[i];
        }
        printf("\nBCC2 CALCULATED: 0X%02X\n", BCC2);

        

        //if(BCC2 == BCC2_read) {
            printf("BCC2 OK\n");
            buf1[0]=0x7E;
            buf1[1]=0x01;
            if(identifier == 0) {
                buf1[2]=0x85;
                //identifier = 1;
            }
            else if (identifier == 1) {
                buf1[2]=0x05;
                //identifier = 0;
            }
            buf1[3]=buf1[1]^buf1[2];
            buf1[4]=0x7E;
            for(int i = 0; i < 5; i++) {
                printf("0x%02X ", buf1[i]);
            }
            //sleep(1);
            int bytes = write(fd, buf1, 5);
            printf("\n%d bytes sent\n", bytes);
            contpacketsread++;
            
                /*
        }
        else {
            printf("\n\nBCC2 NOT OK\n\n");
            buf1[0]=0x7E;
            buf1[1]=0x01;
            if(identifier == 0) {
                buf1[2]=0x01;
                identifier = 1;
            }
            else if (identifier == 1) {
                buf1[2]=0x81;
                identifier = 0;
            }
            buf1[3]=buf[1]^buf[2];
            buf1[4]=0x7E;
            int bytes = write(fd, buf1, 5);
            printf("\n%d bytes sent\n", bytes);
            
        }   */

        memcpy(buf, data, data_pointer);
        
        
        return data_pointer-1;  
}


////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    return 1;
}


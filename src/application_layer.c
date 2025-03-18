// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <math.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    
    LinkLayer connectionParameters; // Configura a camada de ligação de dados (DLL)
   

    strcpy(connectionParameters.serialPort,serialPort);

    // Define o papel (transmissor ou recetor)
    
    if(!strcmp(role, "rx")) connectionParameters.role = LlRx;

    else if(!strcmp(role, "tx")) connectionParameters.role = LlTx;

    else printf("WRONG INITIALIZATION!");
    

    connectionParameters.baudRate = baudRate; // Taxa de transmissão
    connectionParameters.nRetransmissions = nTries; // Número de tentativas de retransmissão
    connectionParameters.timeout = timeout;  // Tempo de espera por uma resposta

    // Abre a ligação com a porta serial
    int fd = llopen(connectionParameters);
    if (fd < 0) {
        perror("Connection error\n");
        exit(-1);
    }
    else {
        printf("\n---------------------------Connection established!---------------------------\n");
    }

    unsigned char control_packet[7+strlen(filename)];
    switch (connectionParameters.role){
        case  LlTx:

            FILE *file;
            file = fopen(filename, "r");

            if (file == NULL) {
                printf("Erro ao abrir o arquivo!\n");
                exit(1);
            }

            fseek(file, 0L, SEEK_END);       // Posiciona no final
            long int file_Size = ftell(file); // Tamanho = posição final
            fseek(file, 0L, SEEK_SET);       // Volta para o início (opcional)
            
            printf("File name: %s\n", filename);
            printf("File size: %ld bytes\n", file_Size);
            
            int numberofpacket = file_Size / 256;
            int remainder = file_Size % 256;
            printf("Number of Packets: %d\n", numberofpacket+1);
            printf("Last Packet Size: %d\n", remainder);
            int cont = 0;

            ////////////////////////////////////////Create and send control packet//////////////////////////////////////////////////////
            control_packet[0] = 2; // C
            control_packet[1] = 1; // T1
            control_packet[2] = strlen(filename); // L1
            for(int i = 0; i < strlen(filename); i++) control_packet[3+i] = filename[i]; // Name
            control_packet[3+strlen(filename)] = 2; // T2
            control_packet[4+strlen(filename)] = 4; // L2
            control_packet[5+strlen(filename)] = numberofpacket +1 ; // Size file_Size / 256;
            control_packet[6+strlen(filename)] = remainder; // Size file_Size % 256;
            
            printf("---------------------------CONTROL PACKET:--------------------------- \n");
            for (int i = 0; i < 7+strlen(filename); i++) printf("0X%02X ", control_packet[i]);
            printf("\n\n");
            llwrite(control_packet, 7+strlen(filename));
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

            while (cont< numberofpacket+1)
            {
                ////////////////////////////////////////Create and send data packet//////////////////////////////////////////////////////
                printf("---------------------------DATA PACKET NUMBER : %d--------------------------- \n", cont+1);
                unsigned char packet[261];
                packet[0] = 1; // C
                packet[1] = cont; // Número de sequência

                if(cont < numberofpacket) packet[2] = 1; //L1
                else packet[2] = 0; 

                if (cont < numberofpacket) packet[3] = 0; //L2
                else packet[3] = remainder;
                
                size_t bytes_read = fread(&packet[4], 1, 256, file);

                // Calcula BCC (XOR dos dados)
                packet[3 + bytes_read] = 0;
                for (int i = 3; i < 3 + bytes_read; i++) {
                    packet[3 + bytes_read] ^= packet[i];
                }
                    llwrite(packet, 261);
                cont++;

                for (int i = 0; i < 261; i++) printf("0X%02X ", packet[i]);  
                /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            }  
               
            printf("\n---------------------------File sent!---------------------------\n");
            fclose(file);
        break;
    


        case LlRx:

        break;
    }
}


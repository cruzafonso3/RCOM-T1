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

#define PACKET_SIZE 256
long int file_Size ;
int numberofpacket;
int remainer;

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

        FILE *file;

        case  LlTx:

            file = fopen(filename, "r");

            if (file == NULL) {
                printf("Erro ao abrir o arquivo!\n");
                exit(1);
            }
            
            fseek(file, 0L, SEEK_END);       // Posiciona no final
            file_Size = ftell(file); // Tamanho = posição final
            fseek(file, 0L, SEEK_SET);       // Volta para o início (opcional)
            
            printf("File name: %s\n", filename);
            printf("File size: %ld bytes\n", file_Size);
            
            numberofpacket = file_Size / PACKET_SIZE;
            remainer = file_Size % PACKET_SIZE;
            printf("Number of Packets: %d\n", numberofpacket+1);
            printf("Last Packet Size: %d\n", remainer);
            int cont = 0;

            ////////////////////////////////////////Create and send control packet//////////////////////////////////////////////////////  
           
            control_packet[0] = 2; // C
            control_packet[1] = 1; // T1
            control_packet[2] = strlen(filename); // L1
            for(int i = 0; i < strlen(filename); i++) control_packet[3+i] = filename[i]; // Name
            control_packet[3+strlen(filename)] = 2; // T2
            control_packet[4+strlen(filename)] = 4; // L2
            control_packet[5+strlen(filename)] = numberofpacket +1 ; // Size file_Size / PACKET_SIZE;
            control_packet[6+strlen(filename)] = remainer; // Size file_Size % PACKET_SIZE;
            control_packet[7+strlen(filename)] = '\0'; // End of string
            
            printf("---------------------------CONTROL PACKET:--------------------------- \n");
            for (int i = 0; i < 8+strlen(filename); i++) printf("0X%02X ", control_packet[i]);
            printf("\n\n");
            if (llwrite(control_packet, 7+strlen(filename))==1){
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            
            while (cont < numberofpacket + 1) {
                
                unsigned char packet[PACKET_SIZE + 8] = {};
                packet[0] = 1; // C 
                packet[1] = 0;// Número de sequência

     //         if (cont == 256) cont =0;
            
                if (cont < numberofpacket) {
                    packet[2] = 1;  
                    packet[3] = 0;  
                } else {
                    packet[2] = 0;  
                    packet[3] = remainer; 
                }
                fseek(file, cont * PACKET_SIZE, SEEK_SET);
            
                size_t bytes_to_read = (cont == numberofpacket) ? remainer : PACKET_SIZE;
                size_t bytes_read = fread(&packet[4], 1, bytes_to_read, file);
            
                unsigned char BCC2 = 0;
                for (int i = 4; i < 4 + bytes_read; i++) {
                    BCC2 ^= packet[i];
                }
                packet[4 + bytes_read] = BCC2;
            
                printf("\nSENDING DATA PACKET NUMBER: %d | BCC2: 0x%02X\n", cont + 1, BCC2);
                

                if(llwrite(packet, 5 + bytes_read)==-1) {
                    printf("Error sending packet nº %d\n", cont);
                    exit(1);
                }
                
                cont++;
                
            }
        }

        ////////////////////////////////////////Create and send control packet//////////////////////////////////////////////////////  
           
        control_packet[0] = 3; // C
        control_packet[1] = 1; // T1
        control_packet[2] = strlen(filename); // L1
        for(int i = 0; i < strlen(filename); i++) control_packet[3+i] = filename[i]; // Name
        control_packet[3+strlen(filename)] = 2; // T2
        control_packet[4+strlen(filename)] = 4; // L2
        control_packet[5+strlen(filename)] = numberofpacket +1 ; // Size file_Size / PACKET_SIZE;
        control_packet[6+strlen(filename)] = remainer; // Size file_Size % PACKET_SIZE;
        control_packet[7+strlen(filename)] = '\0'; // End of string
        
        printf("\n---------------------------CONTROL PACKET:--------------------------- \n");
        for (int i = 0; i < 7+strlen(filename); i++) printf("0X%02X ", control_packet[i]);
        printf("\n\n");
        llwrite(control_packet, 7+strlen(filename));
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        
            
               
            printf("\n---------------------------File sent!---------------------------\n");
            fclose(file);
            llclose(0);
        break;
    

        case LlRx: { 
            unsigned char *packet = (unsigned char *)malloc(MAX_PAYLOAD_SIZE); // Alocação de memória para o pacote
            long int cont = 0;
            int tamanho;

            llread(packet); // Recebe o pacote de controlo
            
            FILE* file = fopen(filename, "wb");
            if (file == NULL) {
                printf("Error opening file for writing!\n");
                return;
            }
            while(1) {
                cont++;
        
                tamanho = llread(packet);
                
                

                printf("\nRECEIVED DATA PACKET NUMBER: %ld\n", cont);
                //printf("Packet size: %d\n", tamanho);

                if(tamanho == -1) {
                    cont--;
                    continue;
                }
                
                if(tamanho<PACKET_SIZE) { 
                    fwrite(packet, 1, tamanho, file);
                    fclose(file);
                    printf("\n---------------------------File received!---------------------------\n");
                    break;
                    }

                fwrite(packet, 1, PACKET_SIZE, file);
            }
            llread(packet); // Recebe o pacote de controlo
            
            llclose(0);
        } 
    }   


    printf("\n/////////////////////////////////////////////////////////////////////\n");
    printf("/////////////////////////////////////////////////////////////////////\n");
    printf("/////////////////////////////////////////////////////////////////////\n\n");
}

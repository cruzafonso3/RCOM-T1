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
    //filename = "ok.txt";
   //unsigned char control_packet[7+strlen(filename)];
   
    switch (connectionParameters.role){ 
        case  LlTx:

            FILE *file;
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
            
            numberofpacket = file_Size / 256;
            remainer = file_Size % 256;
            printf("Number of Packets: %d\n", numberofpacket+1);
            printf("Last Packet Size: %d\n", remainer);
            int cont = 0;

            ////////////////////////////////////////Create and send control packet//////////////////////////////////////////////////////  não está afuncionar em condições
           /*
            control_packet[0] = 2; // C
            control_packet[1] = 1; // T1
            control_packet[2] = strlen(filename); // L1
            for(int i = 0; i < strlen(filename); i++) control_packet[3+i] = filename[i]; // Name
            control_packet[3+strlen(filename)] = 2; // T2
            control_packet[4+strlen(filename)] = 4; // L2
            control_packet[5+strlen(filename)] = numberofpacket +1 ; // Size file_Size / 256;
            control_packet[6+strlen(filename)] = remainer; // Size file_Size % 256;
            control_packet[7+strlen(filename)] = '\0'; // End of string
            */
            //printf("---------------------------CONTROL PACKET:--------------------------- \n");
            //for (int i = 0; i < 7+strlen(filename); i++) printf("0X%02X ", control_packet[i]);
            //printf("\n\n");
            //llwrite(control_packet, 7+strlen(filename));
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            
            while (cont < numberofpacket + 1) {
                unsigned char packet[256 + 8] = {};
                packet[0] = 1; // C (tipo de pacote: dados)
                packet[1] = cont; // Número de sequência
            
                // Define L1 e L2 (tamanho do payload)
                if (cont < numberofpacket) {
                    packet[2] = 1;  // L1 = 1 (indica 256 bytes)
                    packet[3] = 0;  // L2 = 0 (complemento)
                } else {
                    packet[2] = 0;  // L1 = 0 (último pacote)
                    packet[3] = remainer; // L2 = tamanho restante
                }
            
                // Posiciona o ponteiro do arquivo corretamente
                fseek(file, cont * 256, SEEK_SET);
            
                // Lê 256 bytes (ou "remainer" no último pacote)
                size_t bytes_to_read = (cont == numberofpacket) ? remainer : 256;
                size_t bytes_read = fread(&packet[4], 1, bytes_to_read, file);
            
                // Calcula BCC2 (XOR de todos os bytes do payload)
                unsigned char BCC2 = 0;
                for (int i = 4; i < 4 + bytes_read; i++) {
                    BCC2 ^= packet[i];
                }
                packet[4 + bytes_read] = BCC2; // Insere BCC2
                //packet[5 + bytes_read] = 0x7E; // FLAG final
            
                printf("\nSENDING DATA PACKET NUMBER: %d | BCC2: 0x%02X\n", cont + 1, BCC2);
                
                // Envia o pacote
                llwrite(packet, 5 + bytes_read);
                
                cont++;
                
            }
               
            printf("\n---------------------------File sent!---------------------------\n");
            fclose(file);
        break;
    

        
     
      

        case LlRx: { 
            unsigned char *packet = (unsigned char *)malloc(MAX_PAYLOAD_SIZE); // Alocação de memória para o pacote
            long int cont = 0;
            int tamanho;
            FILE* file = fopen(filename, "wb");
            if (file == NULL) {
                printf("Error opening file for writing!\n");
                return;
            }
            while(1) {
                cont++;
        
                tamanho = llread(packet);
                
                

                printf("\nRECEIVED DATA PACKET NUMBER: %ld\n", cont);
                printf("Packet size: %d\n", tamanho);

                if(tamanho<256) { 
                    fwrite(packet, 1, tamanho, file);
                    fclose(file);
                    printf("\n---------------------------File received!---------------------------\n");
                    break;
                    }
                    fwrite(packet, 1, 256, file);
                }
            }
           
        }
        
    }
    



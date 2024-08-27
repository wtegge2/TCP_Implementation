#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#include "tcp.h"

#define RECEIVER_BUFFER_SIZE 600000
#define TOTAL 300

char receiver_buffer[sizeof(Packet)];

struct sockaddr_in address;
socklen_t address_len;

struct sockaddr_in si_me, si_other;
int s, slen;
int rec_bytes;

void diep(char *s) {
    perror(s);
    exit(1);
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    slen = sizeof (si_other);


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");


	/* Now receive data and send acknowledgements */  
    FILE *fp = fopen(destinationFile, "wb");
    if(fp == NULL){
        printf("Error opening file to write \n");
        exit(1);
    }

    char packet_buffer[RECEIVER_BUFFER_SIZE];
    int next = 0;
    int already_acked[TOTAL];           // array that keeps track of already acked packets
    int size[TOTAL];                    // array that keeps track of the size of each packet
    int temp = 0;

    // initialize already_acked array and size array 
    for(int i = 0; i < TOTAL; i++){

        already_acked[i] = 0;
        size[i] = SIZE_OF_DATA;

    }

    address_len = sizeof(address);

    // loop to receieve packets
    while(1){

        // receieve packet and check for errors
        rec_bytes = recvfrom(s, receiver_buffer, sizeof(Packet), 0, (struct sockaddr*)&address, &address_len);
        if(rec_bytes <= 0){
            fprintf(stderr, "Connection has been closed. \n");
            exit(1);

        }

        Packet packet;

        memcpy(&packet, receiver_buffer, sizeof(Packet));
        
        // check if DATA packet
        if(packet.type == DATA){

            // check if sequence number is correct / expected
            if(packet.sequence_number == next){


                memcpy(&packet_buffer[temp*SIZE_OF_DATA], &packet.data, packet.length);

                fwrite(&packet_buffer[temp*SIZE_OF_DATA], sizeof(char), packet.length, fp);

                next = next + 1;

                temp = (temp + 1) % TOTAL;

                // if this packet has been acked, then write it to the file
                while(already_acked[temp] == 1){

                    fwrite(&packet_buffer[temp*SIZE_OF_DATA], sizeof(char), size[temp], fp);

                    already_acked[temp] = 0;

                    temp = (temp + 1) % TOTAL;

                    next = next + 1;

                }
            }

            // if this packet is out of order / early, then put it into the buffer
            else if(packet.sequence_number > next){

                int temp_2 = (temp + packet.sequence_number - next) % TOTAL;

                for(int i = 0; i < packet.length; i++){

                    packet_buffer[temp_2 * SIZE_OF_DATA + i] = packet.data[i];

                }

                already_acked[temp_2] = 1;
                size[temp_2] = packet.length;

            }


            // send cummulative ack
            Packet ack;

            ack.type = ACK;
            ack.AckNum = next;
            ack.length = 0;

            memcpy(receiver_buffer, &ack, sizeof(Packet));

            sendto(s, receiver_buffer, sizeof(Packet), 0, (struct sockaddr *)&address, address_len);

        }
        // if not DATA packet, then check if its a packet to end the communication
        else if(packet.type == DONE){

            Packet ack;
            ack.type = DONE_ACK;
            ack.AckNum = next;
            ack.length = 0;

            memcpy(receiver_buffer, &ack, sizeof(Packet));

            // send packet to acknowledge end of communication
            sendto(s, receiver_buffer, sizeof(Packet), 0, (struct sockaddr *)&address, address_len);

            break;

        }
    }

    // close socket
    close(s);

	printf("%s fully received. \n", destinationFile);

    // close file
    fclose(fp);

    return;

}


int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}

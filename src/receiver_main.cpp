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

// TA SAID YOU NEEDED TO USE THIS FOR RECEIVER!
struct sockaddr_in address;
socklen_t address_len;

struct sockaddr_in si_me, si_other;
int s, slen;
int rec_bytes;

// helper function definitions


void diep(char *s) {
    perror(s);
    exit(1);
}

/*
int receiver_handshake(){

    Packet syn;

    memset((char *) &address, 0, sizeof (address));

    if(recvfrom(s, &syn, sizeof(Packet), 0, (struct sockaddr*) &si_other, (socklen_t*)&slen) == -1){
        diep("Error with receiving a packet! \n");
        return -1;
    }

    printf("len: %d \n", address_len);
    printf("Syn value: %d \n", syn.sequence_number);

    // check if sequence number is set in received syn packet
    if(syn.sequence_number){

        Packet syn_ack;
        srand(0);
        syn_ack.sequence_number = rand();   // generate random y
        syn_ack.AckNum = syn.sequence_number + 1;
        
        syn_ack.window_size = advertised_window;


        printf("Sequence number y = %d \n", syn_ack.sequence_number);
        printf("Sending AckNum back: %d \n", syn_ack.AckNum);
        printf("Advertised Window Size = %d \n", syn_ack.window_size);

        
        if(sendto(s, &syn_ack, sizeof(Packet), 0, (struct sockaddr*) &si_other, slen) == -1){
            diep("Error with sending a packet! \n");
            return -1;
        }

        // save expected sequence number for later
        // LEFT OFF HERE WORKING ON THE RECIEVER CODE!!! FIGURE OUT HOW TO SET THESE VALUES
        expected_sequence_number = syn_ack.AckNum;  // saving x+1
        // rec_acknum_temp = syn_ack.sequence_number;  // saving y

        printf("HERE \n");
        Packet ack;
        
//        if(recvfrom(s, &ack, sizeof(Packet), 0, (struct sockaddr*) &address, &address_len) == -1){
//            perror("Error Receiving \n");
//            diep("Error with receiving a packet! \n");
//            return -1;
//        }
        
        if(recvfrom(s, &ack, sizeof(Packet), 0, (struct sockaddr*) &si_other, (socklen_t*)&slen) == -1){
            perror("Error Receiving \n");
            diep("Error with receiving a packet! \n");
            return -1;

        }

        if(ack.AckNum != (syn_ack.sequence_number + 1)){
            printf("AckNum received: %d \n", ack.AckNum);
            printf("AckNum should be: %d \n", syn_ack.sequence_number + 1);
            printf("Incorrect Ack value received from sender \n");
            return -1;
        }

        // NEED TO DO ANYTHING AFTER CONNECTION IS ESTABLISHED???

        printf("Received 2nd packet... \n");
        printf("Effective window = %d \n", ack.window_size);

    }
    else{
        printf("Expected SYN not received... \n");
        return -1;
    }

    return 0;
}
*/


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

/*
    // do 3-way handshake with sender
    int handshake_flag = 0;
    handshake_flag = receiver_handshake();

    if(handshake_flag == -1){
        printf("Handshake failed. No connection established. \n");
        exit(1);
    }
    printf("Handshake succeeded on receiever! \n");
*/

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
        
        // printf("Received packet with sequence number: %d \n", packet.sequence_number);
        // printf("Packet type is: %d \n", packet.type);

        // check if DATA packet
        if(packet.type == DATA){

            // check if sequence number is correct / expected
            if(packet.sequence_number == next){


                memcpy(&packet_buffer[temp*SIZE_OF_DATA], &packet.data, packet.length);

                fwrite(&packet_buffer[temp*SIZE_OF_DATA], sizeof(char), packet.length, fp);

                // printf("Writing packet with sequence number: %d \n", packet.sequence_number);
                
                next = next + 1;

                temp = (temp + 1) % TOTAL;

                // if this packet has been acked, then write it to the file
                while(already_acked[temp] == 1){

                    fwrite(&packet_buffer[temp*SIZE_OF_DATA], sizeof(char), size[temp], fp);

                    // printf("Index: %d \n", temp);

                    already_acked[temp] = 0;

                    temp = (temp + 1) % TOTAL;

                    next = next + 1;

                }
            }

            // if this packet is out of order / early, then putit into the buffer
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

            // printf("Sent ack packet with ackNum: %d \n", ack.AckNum);


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

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}

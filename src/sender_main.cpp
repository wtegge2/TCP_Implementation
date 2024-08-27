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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>


#include <queue>

using namespace std;

#include "tcp.h"

#define SENDER_BUFFER_SIZE 300
#define RTT_VALUE 1000*20
#define SLOW_START_STATE 4
#define CONGESTION_STATE 5
#define FAST_RECOVERY_STATE 6
#define WAITING_STATE 7


char sender_window[sizeof(Packet)];     

queue<Packet> packet_buffer;

queue<Packet> waiting_buffer;

// GLOBAL
unsigned long long int num_bytes;
unsigned long long int bytes;
FILE *fp;
unsigned long long int sequence_number;



double cwnd = 1.0;

int ssthread = 64;

int duplicate_acks = 0;

int curr_congestion_state = SLOW_START_STATE;


struct sockaddr_in si_other;
int s, slen;


// helper function definitions
void congestion(bool timeout_happened, bool acknowledgment);

void socket_timeout(int s_);

void send_packets(int socket);

int populate_buf(int packets);


void diep(char *s) {
    perror(s);
    exit(1);
}


void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    //Open the file
    
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

    num_bytes = bytesToTransfer;

    // initializing the socket 
    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    // fill buffer 
    populate_buf(SENDER_BUFFER_SIZE);

    // HANDLE TIMEOUT STUFF HERE
    socket_timeout(s);

    // send packets containing the specified file
    send_packets(s);

    // loop until the buffer is empty or there are no packets waiting to be sent
    while(!packet_buffer.empty() || !waiting_buffer.empty()){
        
        // check if we received a packet
        if((bytes = recvfrom(s, sender_window, sizeof(Packet), 0, NULL, NULL)) == -1){

            if(errno != EAGAIN || errno != EWOULDBLOCK){
                perror("Couldn't get ack \n");
                exit(1);
            }

            // timeout - so we put the packet into the waiting buffer
            memcpy(sender_window, &waiting_buffer.front(), sizeof(Packet));

            
            if((bytes = sendto(s, sender_window, sizeof(Packet), 0, (struct sockaddr*) &si_other, slen)) == -1){

                perror("Error sending data! \n");

                exit(1);

            }

            congestion(true, false);

        }
        else{

            Packet packet;

            memcpy(&packet, sender_window, sizeof(Packet));

            if(packet.type == ACK){

                if(packet.AckNum == waiting_buffer.front().sequence_number){

                    congestion(false, false);

                    if(duplicate_acks == 3){

                        ssthread = cwnd/2.0;
                        cwnd = ssthread + 3;
                        duplicate_acks = 0;

                        // resend duplicate packets
                        memcpy(sender_window, &waiting_buffer.front(), sizeof(Packet));
                        if((bytes = sendto(s, sender_window, sizeof(Packet), 0, (struct sockaddr*) &si_other, slen)) == -1){

                            perror("Error sending data! \n");
    
                            exit(1);

                        }

                    }
                }
                else if(packet.AckNum > waiting_buffer.front().sequence_number){
                    while(!waiting_buffer.empty() && waiting_buffer.front().sequence_number < packet.AckNum){

                        congestion(false, true);
                        
                        waiting_buffer.pop();

                    }

                    send_packets(s);

                }
            }
        }

    }

    // close file
    fclose(fp);

    Packet packet;

    // 3-way handshake to shut down connection
    while(1){

        packet.type = DONE;
        packet.length = 0;
        memcpy(sender_window, &packet, sizeof(Packet));

        if((bytes = sendto(s, sender_window, sizeof(Packet), 0, (struct sockaddr*) &si_other, slen)) == -1){
            perror("Can't send DONE packet to sender \n");
            exit(1);

        }

        Packet ack;

        if((bytes = recvfrom(s, sender_window, sizeof(Packet), 0, (struct sockaddr*) &si_other, (socklen_t*)&slen) == -1)){
            perror("Can't receive \n");
            exit(1);
        }

        memcpy(&ack, sender_window, sizeof(Packet));

        if(ack.type == DONE_ACK){

            printf("Done sending!!! \n");

            break;

        }

        
    }

	/* Send data and receive acknowledgements on s*/
    // close socket 
    printf("Closing the socket\n");
    close(s);


    return;

}

int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);

    return (EXIT_SUCCESS);
}

// helper function to handle congestion - handles slow start, timeouts, and fast recovery
void congestion(bool timeout_happened, bool acknowledgment){

    // checks what the currect state of the connection is and acts on that
    // state can be slow start, fast recovery, or normal congestion 
    // checks and handles timeouts in each state
    switch(curr_congestion_state){

        // need to handle slow start
        case SLOW_START_STATE:

            // check for a timeout
            if(timeout_happened){
                
                // reset ssthread
                ssthread = cwnd / 2.0;

                // reset control window
                cwnd = 1;

                // reset number of duplicate acks
                duplicate_acks = 0;

                return;

            }

            // check if new ack
            if(acknowledgment){
                
                // reset number of duplicate acks
                duplicate_acks = 0;

                // resize control window
                cwnd = (cwnd + 1 >= SENDER_BUFFER_SIZE) ? SENDER_BUFFER_SIZE - 1 : cwnd + 1;

            }
            else{
                
                // increment number of duplicate acks
                duplicate_acks = duplicate_acks + 1;

            }

            // check if our control window is greater than or equal to our threshhold
            if(cwnd >= ssthread){
                
                // go to CONGESTION STATE 
                curr_congestion_state = CONGESTION_STATE;

            }
            break;
        
        // need fast recovery
        case FAST_RECOVERY_STATE:

            // check for a timeout
            if(timeout_happened){

                // reset ssthread
                ssthread = cwnd / 2.0;

                // reset control window
                cwnd = 1;

                // reset number of duplicate acks
                duplicate_acks = 0;

                // printf("Congestion Window is: %f", cwnd);

                // go to SLOW START state next
                curr_congestion_state = SLOW_START_STATE;

            }

            // check if new ack
            if(acknowledgment){

                // set control window equal to ssthread
                cwnd = ssthread;

                // reset number of duplicate acks
                duplicate_acks = 0;

                // go to CONGESTION STATE next
                curr_congestion_state = CONGESTION_STATE;
                
            }
            else{
                
                // resize control window
                cwnd = (cwnd + 1 >= SENDER_BUFFER_SIZE) ? SENDER_BUFFER_SIZE - 1 : cwnd + 1;

            }
            break;

        // need to handle congestion
        case CONGESTION_STATE:

            // check for a timeout
            if(timeout_happened){

                // reset ssthread 
                ssthread = cwnd / 2.0;

                // reset control window
                cwnd = 1;

                // reset number of duplicate acks
                duplicate_acks = 0;

                // go to SLOW START state next
                curr_congestion_state = SLOW_START_STATE;

            }

            // check if new ack
            if(acknowledgment){
                
                // resize control window
                cwnd = (cwnd + 1.0/cwnd >= SENDER_BUFFER_SIZE) ? SENDER_BUFFER_SIZE - 1 : cwnd + 1.0/cwnd;
                
                // reset number of duplicate acks
                duplicate_acks = 0;
            
            }
            else{

                // increment number of duplicate acks
                duplicate_acks = duplicate_acks + 1;

            }
            break;

        default:

            // do nothing in default state
            break;    
    }


}

// helper function to setup the timeout for a given socket
void socket_timeout(int s_){

    // sets timer to RTT * 2
    struct timeval RTT_TIMEOUT;
    RTT_TIMEOUT.tv_sec = 0;
    RTT_TIMEOUT.tv_usec = 2*RTT_VALUE;

    // set timeout on socket
    if(setsockopt(s_, SOL_SOCKET, SO_RCVTIMEO, &RTT_TIMEOUT, sizeof(RTT_TIMEOUT)) < 0){

        fprintf(stderr, "Error with the socket timeout \n");
        
        return;
    
    }

}

// helper function to send packets to receiver
// TA said do it this way instead of in the transfer function 
void send_packets(int socket){

    int packets;
    int waiting_buffer_size = waiting_buffer.size();
    int packet_buffer_size = packet_buffer.size();

    // get number of packets we have left to send
    packets = (cwnd - waiting_buffer_size) <= packet_buffer_size ? cwnd - waiting_buffer_size : packet_buffer_size;

    // check if our control window size is equal or smaller than our waiting buffer
    if(cwnd - waiting_buffer_size < 1){

        // put first packet of our waiting buffer into the send buffer
        memcpy(sender_window, &waiting_buffer.front(), sizeof(Packet));

        // send packet to receiver
        if((bytes = sendto(s, sender_window, sizeof(Packet), 0, (struct sockaddr*) &si_other, slen)) == -1){
            
            perror("Error sending packet! \n");

            exit(1);

        }

        return;

    }

    // check if we have packets to send or not
    if(packet_buffer.empty()){

        return;

    }

    // loop for every packet 
    for(int i = 0; i < packets; i++){

        // copy first packet in the packet buffer to the sender window buffer
        memcpy(sender_window, &packet_buffer.front(), sizeof(Packet));

        // send packet to receiver
        if((bytes = sendto(s, sender_window, sizeof(Packet), 0, (struct sockaddr*) &si_other, slen)) == -1){
            
            perror("Error sending packet! \n");

            exit(1);
            
        }

        // put packet that was just sent into the waiting buffer (queue)
        waiting_buffer.push(packet_buffer.front());

        // remove packet that was just sent from the packet buffer
        packet_buffer.pop();

    }

    // create more packets with data 
    populate_buf(packets);

}


// helper function to break file up into packets and fill the buffer up with these packets
int populate_buf(int packets){

    // if argument is 0, return 0
    if(packets == 0){
        return 0;
    }

    int byte;
    char temp_buf[SIZE_OF_DATA];
    int cnt = 0;
    int size;


    for(int i = 0; num_bytes != 0 && i < packets; i++){

        Packet packet;

        // check if the number of bytes is less than our max data size per packet
        if(num_bytes < SIZE_OF_DATA){

            byte = num_bytes;
        
        }
        else{
            
            // if the number of total bytes is greater than our packet data size, then use the max data size
            byte = SIZE_OF_DATA;

        }

        // read amount of specified bytes from file into our buffer
        size = fread(temp_buf, sizeof(char), byte, fp);

        // check if bytes were read from file
        if(size > 0){
            
            // set values in packet and send it 
            packet.length = size;

            packet.type = DATA;

            packet.sequence_number = sequence_number;

            memcpy(packet.data, &temp_buf, sizeof(char)*byte);

            // put packet into our buffer to wait for its ack from receiver
            packet_buffer.push(packet);

            // set our next sequence number 
            sequence_number = (sequence_number + 1) % MAX_SEQUENCE_NUMBER;

        }

        // adjust out number of bytes left to send
        num_bytes -= size;

        // keep track of number of packets filled and sent
        cnt = i;

    }

    // return number of packets that were filled with data
    return cnt;

}
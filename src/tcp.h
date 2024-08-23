#ifndef TCP_H
#define TCP_H

#include <stddef.h>
#include <time.h>

#define SIZE_OF_DATA 2000

#define DATA 0
#define ACK 1
#define DONE 2
#define DONE_ACK 3



#define SIZE_OF_WINDOW 10

#define MAX_SEQUENCE_NUMBER 100000


typedef struct {

    int sequence_number;
    int AckNum;
    int length;
    char data[SIZE_OF_DATA];
    clock_t time_sent;
    int window_size;
    int type; 

} Packet;

// sender
extern char sender_window[sizeof(Packet)];
extern int LastByteAcked;
extern int LastByteSent;
extern int LastByteWritten;
extern unsigned int next_sequence_number;
extern unsigned int effective_window;

// receiver
extern char receive_buffer[sizeof(Packet)];
extern int LastByteRead;
extern int NextByteExpected;
extern int LastByteRcvd;
extern unsigned int expected_sequence_number;
extern unsigned int advertised_window;




#endif /* TCP_H */
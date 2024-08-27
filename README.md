# TCP_Implementation

This is an implementation of a transport protocol (similar to TCP). I say similar to TCP because it has properties of TCP, but it uses the UDP protocol in order to understand reliable packet transfer. 

To read more about how TCP works, click [here](https://book.systemsapproach.org/e2e/tcp.html)

This version of TCP is capable of the following:
    
    - tolerates packet drops
    - allows other concurrent connections a fair chance 
    - does not give up the entire bandwidth to other connections

#ifndef gbn_h
#define gbn_h

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>


/*Packet/State types*/
#define SYN 0                  
#define SYNACK 1
#define DATA 2
#define ACK 3
#define FIN 4
#define FINACK 5


/*Transport protocol header*/
typedef struct rtp_struct {
    int flags;
    int id;
    int seq;
    int windowsize;
    int checksum;
    char* data;
} rtp;


int threeWayHandshake(int sock, struct sockaddr_in serverName);
int teardown(int sock, struct sockaddr_in serverName);

int checksum(uint16_t *buffer, int len);
int validate_checksum(rtp *packet);

#endif
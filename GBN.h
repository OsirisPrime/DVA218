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


#define hostNameLength 50
#define windowSize 1
#define MAXMSG 1024


/* Packet flags */
#define SYN 0                  
#define SYNACK 1
#define DATA 2
#define ACK 3
#define FIN 4
#define FINACK 5


int s_state;
int r_state;

/* Sender connection states */
#define CLOSED 0
#define WAIT_SYNACK 1
#define RCVD_SYNACK 2
#define ESTABLISHED 3

/* Reciever connection states */
#define LISTENING 0
#define SENT_SYNACK 1
#define WAIT_ACK 2
#define ESTABLISHED 3

/* Other states */
#define WAIT_ACK 1
#define WAIT_FIN 2
#define WAIT_FINACK 3


/* ransport protocol header */
typedef struct rtp_struct {
    int flags;
    //int id; If there are more then one connections
    int seq;
    int windowsize;
    int checksum;
    char* data;
} rtp;


int sender_connection(int sockfd, const struct sockaddr *serverName);
int receiver_connection(int sockfd);

int teardown(int sock, struct sockaddr_in serverName);

uint16_t checksum(rtp *packet);
int validate_checksum(rtp *packet);

#endif

/*
===============================================================================
File Name       : gbn.h
Authurs         : Kim Svedberg & Zebastian Thorsén 
Version         : 1.0
Date            : 2024-05-21
Description     : Header file for a reliable GBN transfer protocol on-top of UDP.
                  Contains all function definitions & packet info.
===============================================================================
*/

#ifndef gbn_h
#define gbn_h

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include <semaphore.h>


/*----- Protocal parameters -----*/
#define PORT 5555                   /* Socket number */
#define hostNameLength 50           /* The lenght of host name*/
#define windowSize 3                /* Sliding window size */
#define MAXMSG 255                  /* Maximun data to be sent once*/
#define packetToSend 100             /* The number of packet to send */
#define LOSS_PROB 5e-2                 /* Packet loss probability */
#define CORR_PROB 5e-2                 /* Packet corrution probability */
#define TIMEOUT 5                   /* Timeout time (5s) */


/*----- Packet flags -----*/
#define SYN 0                  
#define SYNACK 1
#define DATA 2
#define ACK 3
#define FIN 4
#define FINACK 5


/*----- All possible states -----*/
enum states {
    CLOSED,
    WAIT_SYNACK,
    ESTABLISHED,
    WAIT_OPEN,
    SEND_DATA,
    WAIT_FINACK,
    LISTENING,
    WAIT_CLOSE,
    WAIT_OPEN_ACK,
    WAIT_CLOSE_ACK,
};


/*----- Transport protocol header -----*/
typedef struct rtp_struct {
    int flags;
    int seq;
    int id;
    int winSize;
    int checksum;
    char data[MAXMSG];
} rtp;


/*----- State information -----*/
typedef struct states_t{
    int state;
    int seqnum;
    int window_size;
    int sockfd;
    struct sockaddr *DestName;
    struct sockaddr *OwnName;
    socklen_t socklen;

} state_t;


/*----- All function for the protocol -----*/
int sender_connect(int sockfd, const struct sockaddr *serverName, socklen_t socklen);
int receiver_connect(int sockfd, const struct sockaddr *client, socklen_t *socklen);

ssize_t sender_gbn(int sockfd, const void *buf, size_t len, int flags);
ssize_t receiver_gbn(int sockfd, void *buf, size_t len, int flags);
ssize_t maybe_sendto(int sockfd, const void *buf, size_t len, int flags,
                     const struct sockaddr *to, socklen_t tolen);

int sender_teardown(int sockfd, const struct sockaddr *serverName, socklen_t socklen);
int receiver_teardown(int sockfd, const struct sockaddr *client, socklen_t socklen);

rtp packetBuild(rtp packet, int type);
void signal_handler(int signum);
unsigned short checksum(const rtp *packet);
int verify_checksum(const rtp *packet);

#endif

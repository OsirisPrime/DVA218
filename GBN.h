/* File: GBN.h
 * Authors: Kim Svedberg, Zebastian Thorsén 
 * Description:
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
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>


/* Protocal parameters */
#define hostNameLength 50   /* The lenght of host name*/
#define windowSize 1        /* Sliding window size */
#define MAXMSG 1024         /* Maximun data to be sent once*/
#define LOSS_PROB 1e-2      /* Packet loss probability */
#define CORR_PROB 1e-3      /* Packet corrution probability */

/* Packet flags */
#define SYN 0                  
#define SYNACK 1
#define DATA 2
#define ACK 3
#define FIN 4
#define FINACK 5

int s_state;            /* Sender state */
int r_state;            /* Receiver state */

/* All possible states */
enum states {
    CLOSED,
    WAIT_SYNACK,
    RCVD_SYNACK,
    ESTABLISHED,
    WAIT,
    SEND_DATA,
    PACKET_LOSS,
    WAIT_FINACK,
    RCVD_FINACK,
    WAIT_TIME,
    LISTENING,
    SENT_SYNACK,
    WAIT_ACK,
    READ_DATA,
    DEFAULT,
    RCVD_FIN,
};

/* Transport protocol header */
typedef struct rtp_struct {
    int flags;
    //int id; If there are more then one connections
    int seq;
    int windowsize;
    int checksum;
    char data[MAXMSG];
} rtp;

/* All function for the protocol */
int sender_connection(int sockfd, const struct sockaddr *serverName, socklen_t socklen);
int receiver_connection(int sockfd, const struct sockaddr *client, socklen_t *socklen);

int sender_teardown(int sockfd, const struct sockaddr *serverName, socklen_t socklen);
int receiver_teardown(int sockfd, const struct sockaddr *client, socklen_t socklen);

uint16_t checksum(rtp *packet);
int validate_checksum(rtp *packet);

#endif

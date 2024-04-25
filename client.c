/* File: client.c
 * Authors: Kim Svedberg, Zebastian Thorsén 
 * Description:
 */

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

#define PORT 5555
#define hostNameLength 50
#define windowSize 1
#define MAXMSG 1024


/*Transport protocol header*/
typedef struct rtp_struct {
    int ACK;
    int SYN;
    int SYN_ACK;
    int FIN;
    int FIN_ACK;
    int id;
    int seq;
    int windowsize;
    int crc;
    char* data;
} rtp;


/*Initiat a socket given a host name and a port*/
void initSocketAddress(struct sockaddr_in *name, char *hostName, unsigned short int port){
    struct hostent *hostInfo; /*Contains info about the host*/

    name->sin_family = AF_INET; /*Make socket use IPv4*/
    name->sin_port = htons(port); /*Set port number in network byte order*/

    hostInfo = gethostbyname(hostName); /*Get info about the host*/
    /*If we couldn't get info about the host*/
    if(hostInfo == NULL){
        fprintf(stderr, "initSocketAdress - Unknown host %s\n", hostName);
        exit(EXIT_FAILURE);
    }

    /*Fill in the host name into the socketaddr_in struct*/
    name->sin_addr = *(struct in_addr *)hostInfo->h_addr;
}


void threeWayHandshake(int sock, struct sockaddr_in serverName){
    struct timeval timeout;
    char buffer[MAXMSG];
    srand(time(NULL));
    int nOfBytes;
    rtp packet;    
    fd_set activeFdSet;

    /*Timeout values*/
    timeout.tv_sec = 0;
    timeout.tv_usec = 100;
    int seqNum = (rand() % 10000) + 10;


    
    packet.SYN = 1;
    packet.seq = seqNum;
    packet.windowsize = windowSize;

    nOfBytes = sendto(sock, &packet, sizeof(rtp), 0, (struct sockaddr *)&serverName, sizeof(serverName));
    if(nOfBytes < 0){
        perror("theeWayHandshake - Could not send SYN packet\n");
        exit(EXIT_FAILURE);
    }

    FD_Zero(&activeFdSet);
    FD_SET(sock, &activeFdSet);

    /*SYN_ACK Wait state*/
    while(1){
        int result = select(sock + 1, &activeFdSet, NULL, NULL, &timeout);

        if(result == -1){
            perror("Select failed\n");
            exit(EXIT_FAILURE);
        }
        else if(result == 0){
            printf("threeWayHandshake - SYN packet loss");
            nOfBytes = sendto(sock, &packet, sizeof(rtp), 0, (struct sockaddr *)&serverName, sizeof(serverName));
            if(nOfBytes < 0){
                perror("theeWayHandshake - Could not send SYN packet\n");
                exit(EXIT_FAILURE);
            }            
        }
        else{
            break;
        }
    }

    /*Receive packet from server*/
    nOfBytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&serverName, sizeof(serverName));

    memcpy(&packet, buffer, sizeof(rtp));

    /*Check if packet is ACK*/
    if(packet.SYN != 1 || packet.ACK != 1){

    }


}

int teardown(int sock, struct sockaddr_in serverName){

}


int main(int argc, char *argv[]){
    int sock;
    struct sockaddr_in serverName;
    char hostName[hostNameLength];

    /*Check arguments*/
    if(argv[1] == NULL){
        perror("Usage: client [host name]\n");
        exit(EXIT_FAILURE);
    }
    else{
        strncpy(hostName, argv[1], hostNameLength);
        hostName[hostNameLength - 1] = '\0';
    }

    /*Create the socket*/
    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if(sock < 0){
        perror("Could not create a socket\n");
        exit(EXIT_FAILURE);
    }

    /*Initialize the socket address*/
    initSocketAddress(&serverName, hostName, PORT);

    /*Start a connection to the server*/
    threeWayHandshake(sock, serverName);



}
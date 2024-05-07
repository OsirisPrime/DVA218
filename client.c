/* File: client.c
 * Authors: Kim Svedberg, Zebastian Thorsén 
 * Description:
 */

#include "GBN.h"

#define PORT 5555
#define hostNameLength 50
#define windowSize 1
#define MAXMSG 1024



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
    if(threeWayHandshake(sock, serverName) == -1){
        perror("threeWayHandshake");
        exit(EXIT_FAILURE);
    }

    /*Send all packet*/
    while(1){
        
    }

    /*Close the socket*/
    if (teardown(sock, serverName) == -1){
        perror("teardown");
        exit(EXIT_FAILURE);
    }


    return 0;
}
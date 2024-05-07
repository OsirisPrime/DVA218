/* File: server.c
 * Authors: Kim Svedberg, Zebastian Thorsén 
 * Description:
 */

#include "GBN.h"

#define PORT 5555
#define MAXMSG 512


int makeSocket(unsigned short int port){
    int sock;
    struct sockaddr_in name;

    /*Create a socket*/
    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if(sock < 0){
        perror("Could not create a socket\n");
        exit(EXIT_FAILURE);
    }

    name.sin_family = AF_INET; /*Give the socket a name*/
    name.sin_port = htons(port); /*Set sockets port number in network byte order*/
    name.sin_addr.s_addr = htonl(INADDR_ANY); /*Set the internet addres of the host*/

    /*Assign an address to the socket by calling bind*/
    if(bind(sock, (struct sockaddr *)&name, sizeof(name)) < 0){
        perror("Could not bind a name to the socket\n");
        exit(EXIT_FAILURE);
    }
    
    return(sock);
}



int main(int argc, char *argv[]){
    int sock;
    int clientSocket;
    struct sockaddr_in clientName;
    socklen_t size;

    /*Create a socket and set it up to accept connections*/
    sock = makeSocket(PORT);

    printf("\n[Waiting for connection...]\n");

    threeWayHandshake(sock);

    /*Read all packets*/
    while(1){

    }

    /*Close the socket */


    return 0;
}
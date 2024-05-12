/* File: server.c
 * Authors: Kim Svedberg, Zebastian Thorsén 
 * Description: File containing the receiver code.
 */

#include "GBN.h"

#define PORT 5555


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
    int sockfd;                     /* Socket file descriptor of the receiver */              
    int numRead;                    /* Number of read bytes */
    char buf[MAXMSG];
    socklen_t socklen;              /* Length of the socket structure sockaddr */
    struct sockaddr_in clientName;

    /* Create a socket and set it up to accept connections */
    sockfd = makeSocket(PORT);

    /* Listening */
    printf("\n[Waiting for connection...]\n");

    socklen = sizeof(struct sockaddr_in);
    int new_sockfd = receiver_connection(sockfd, (struct sockaddr*)&clientName, &socklen);
    if(new_sockfd == -1){
        perror("receiver_connection");
        exit(EXIT_FAILURE);
    }

    /* Read all packets */
    while(1){
        if((numRead = receiver_gbn(new_sockfd, buf, MAXMSG, 0)) == -1){
            perror("receiver_gbn");
            exit(EXIT_FAILURE);

        } else if(numRead == 0){
            break;
        }

    }

    /* Close the socket */
    if(receiver_teardown(new_sockfd, (struct sockaddr*)&clientName, socklen) == -1){
        perror("receiver_teardown");
        exit(EXIT_FAILURE);
    }

    return 0;
}

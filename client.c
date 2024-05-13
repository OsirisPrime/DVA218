/* File: client.c
 * Authors: Kim Svedberg, Zebastian Thorsén 
 * Description: File containing the sender code.
 */

#include "GBN.h"

#define PORT 5555




int sender_connection(int sockfd, const struct sockaddr *serverName, socklen_t socklen){
    char buffer[MAXMSG];
    fd_set activeFdSet;
    srand(time(NULL));
    int nOfBytes = 0;
    int result = 0;

    s_state = CLOSED;

    /* Timeout */
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    

    /* Initialize SYN_packet */
    rtp *SYN_packet = malloc(sizeof(*SYN_packet));
    if(SYN_packet == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    SYN_packet->flags = SYN;    //SYN packet                     
    SYN_packet->seq = (rand() % (MAX_SEQ_NUM - MIN_SEQ_NUM + 1)) + MIN_SEQ_NUM;    //Chose a random seq_number between 5 & 99
    SYN_packet->windowsize = windowSize;
    memset(SYN_packet->data, '\0', sizeof(SYN_packet->data));
    SYN_packet->checksum = checksum(SYN_packet);


    /* Initilize SYNACK_packet */
    rtp *SYNACK_packet = malloc(sizeof(*SYNACK_packet));
    if(SYNACK_packet == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memset(SYNACK_packet->data, '\0', sizeof(SYNACK_packet->data));
    struct sockaddr from;
    socklen_t from_len = sizeof(from);


    /* Initialize ACK_packet */
    rtp *ACK_packet = malloc(sizeof(*ACK_packet));
    if(ACK_packet == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    ACK_packet->flags = ACK;
    memset(ACK_packet->data, '\0', sizeof(ACK_packet->data));


    FD_ZERO(&activeFdSet);
    FD_SET(sockfd, &activeFdSet);

    /* State machine */
    while(1){
        switch(s_state){

            /* Start state, begins with a closed connection. Send SYN */
            case CLOSED:
                printf("Sending SYN packet\n");

                /* Send SYN_packet to receiver */
                nOfBytes = sendto(sockfd, SYN_packet, sizeof(*SYN_packet), 0, serverName, socklen);
                
                /* Failed to send SYN_packet to the receiver */
                if(nOfBytes < 0){
                perror("Sender connection - Could not send SYN packet\n");
                exit(EXIT_FAILURE);
                }

                /* If all went well, switch to next state */
                s_state = WAIT_SYNACK;
                break;
            
            /* Wait for an ACK (SYNACK) for the SYN */
            case WAIT_SYNACK:

                timeout.tv_sec = 5;
                timeout.tv_usec = 0;

                result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);

                if(result == -1){   //Select fails
                    perror("WAIT_SYN select failed");
                    exit(EXIT_FAILURE);

                } else if(result == 0){ //If a timeout occurs that means the packet was lost
                    printf("TIMEOUT: SYN packet lost\n");
                    s_state = CLOSED;
                    break;

                } else{ /* Receives a packet */
                    if(recvfrom(sockfd, SYNACK_packet, sizeof(*SYNACK_packet), 0, &from, &from_len) != -1){
                        printf("New packet arrived!\n");
                        

                        /* If the packet is a SYNACK and have a valid checksum*/
                        if(SYNACK_packet->flags == SYNACK && SYNACK_packet->checksum == checksum(SYNACK_packet)){
                            printf("Valid SYNACK packet!\n");
                            printf("Packet info - Type: %d\tSeq: %d\n", SYNACK_packet->flags, SYNACK_packet->seq);

                            /* Finalize the ACK_packet */
                            ACK_packet->seq = SYNACK_packet->seq + 1;
                            ACK_packet->checksum = checksum(ACK_packet);
                            
                            /* Switch to next state */
                            s_state = RCVD_SYNACK;

                        } else{ /* If the packet is not a SYNACK or a mismatch of checksum detected */
                            printf("Invalid SYNACK packet!\n");
                            printf("Retransmitting SYN packet\n");
                            s_state = CLOSED;
                        }
                    } else{ /* Couldn't read from socket */
                        perror("Can't from read socket");
                        exit(EXIT_FAILURE);
                    }  
                }
                break;

            /* Received SYNACK, send an ACK for that*/
            case RCVD_SYNACK:
                nOfBytes = sendto(sockfd, ACK_packet, sizeof(*ACK_packet), 0, serverName, socklen);
                
                /* Failed to send ACK to the receiver */
                if(nOfBytes < 0){
                perror("Sender connection - Could not send SYN packet\n");
                exit(EXIT_FAILURE);
                }

                printf("ACK sent!\n");

                /* If successfully sent, switch to next state */
                s_state = ESTABLISHED;
                break;

            /* Sent an ACK for the SYNACK, wait for retransmissions */
            case ESTABLISHED:

                timeout.tv_sec = 5;
                timeout.tv_usec = 0;

                result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);

                if(result == -1){   //Select fails
                    perror("ESTABLISHED select failed");
                    exit(EXIT_FAILURE);

                } else if(result == 0){ //Timeout occurs, no packet loss
                    printf("TIMEOUT occured\n");
                    printf("Connection succeccfully established\n\n");
                    
                    /* Free all allocated memory */
                    free(SYN_packet);
                    free(SYNACK_packet);
                    free(ACK_packet);
                    return 1;   /* Return that connection was made (maybe the seq of client) */

                } else{ //Receives SYNACK again, ACK packet was lost
                    printf("SYNACK arrived again\n");

                    /* Swith to the previous state */
                    s_state = RCVD_SYNACK;
                }
                break;

            default:
                break;
        }
    }
}

int sender_teardown(int sockfd, const struct sockaddr *serverName, socklen_t socklen){
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    fd_set activeFdSet;
    int nOfBytes;
    int result;

    /* Timeout */
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    /* Initilize FIN packet */
    rtp *FIN_packet = malloc(sizeof(*FIN_packet));
    FIN_packet->flags = FIN;
    memset(FIN_packet->data, '\0', sizeof(FIN_packet->data));
    FIN_packet->checksum = checksum(FIN_packet);

    /* Initilize FINACK packet */
    rtp *FINACK_packet = malloc(sizeof(*FINACK_packet));
    memset(FINACK_packet->data, '\0', sizeof(FINACK_packet));

    /* Initilize ACK packet */
    rtp *ACK_packet = malloc(sizeof(*ACK_packet));
    ACK_packet->flags = ACK;
    memset(ACK_packet->data, '\0', sizeof(ACK_packet));

    FD_ZERO(&activeFdSet);
    FD_SET(sockfd, &activeFdSet);

    /* State machine */
    while(1){
        switch (s_state){

            /* Start state. The connection is established, send FIN */
            case ESTABLISHED:
                printf("Sending FIN packet\n");
                nOfBytes = sendto(sockfd, FIN_packet, sizeof(*FIN_packet), 0, serverName, socklen);
                
                /* Failed to send FIN_packet to receiver */
                if(nOfBytes < 0){
                    perror("Send FIN packet fail\n");
                    exit(EXIT_FAILURE);
                }

                /* Successfully sent FIN_packet, switch to next state */
                s_state = WAIT_FINACK;
                break;

            /* Wait for an ACK (FINACK) for the FIN*/
            case WAIT_FINACK:
                /* Look if a packet has arrived */
                result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);
                
                if(result == -1){   /* Select failed */
                    perror("select failed");
                    exit(EXIT_FAILURE);

                } else if(result == 0){ /* Timeout occurs, packet was lost */
                    printf("TIMEOUT - FIN packet loss\n");
                    s_state = ESTABLISHED;

                } else{ /* Receives a packet */
                    /* Can read from socket */
                    if(recvfrom(sockfd, FINACK_packet, sizeof(*FINACK_packet), 0, &from, &fromlen) != -1){
                        printf("New packet arrived!\n");

                        /* Correct packet type and checksum */
                        if(FINACK_packet->flags == FINACK && FINACK_packet->checksum == checksum(FINACK_packet)){
                            
                            printf("Valid FINACK packet!\n");
                            printf("Packet info - Type: %d\tSeq: %d\n", FINACK_packet->flags, FINACK_packet->seq);                            
                            
                            /* Finalize the ACK_packet */
                            ACK_packet->seq = FINACK_packet->seq + 1;
                            ACK_packet->checksum = checksum(ACK_packet);

                            /* Switch to next state */
                            s_state = RCVD_FINACK;

                        } else{ /* Invalid packet type or checksum */
                            printf("Invalid FINACK packet\n");
                            printf("Retransmitting FIN packet\n");
                            /* Switch to previous state */
                            s_state = ESTABLISHED;
                        }

                    } else{ /* Can't read from socket */
                        perror("Can't read socket");
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case RCVD_FINACK:
                result = sendto(sockfd, ACK_packet, sizeof(*ACK_packet), 0, serverName, socklen);
                
                /* Failed to send ACK */
                if(result < 0){
                    perror("Send ACK-packet failed");
                    exit(EXIT_FAILURE);
                }

                /* Sent ACK, switch to next state */
                printf("ACK sent!\n");
                s_state = WAIT_TIME;
                break;

            case WAIT_TIME:
                /* Look if a packet has arrived */
                result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);
                
                if(result == -1){   /* Select failed */
                    perror("select failed");
                    exit(EXIT_FAILURE);

                } else if(result == 0){ /* Timeout occurs, packet arrived */
                    printf("TIMEOUT occurred\n");

                    /* Switch to next state */
                    s_state = CLOSED;
                    break;

                } else{ /* Receives a new packet, ACK was lost */
                    printf("Packet arrived again!\n");

                    if(recvfrom(sockfd, FINACK_packet, sizeof(*FINACK_packet), 0, &from, &fromlen) != -1){
                        printf("New packet arrived!\n");

                        /* Correct packet type and checksum */
                        if(FINACK_packet->flags == FINACK && FINACK_packet->checksum == checksum(FINACK_packet)){
                            
                            printf("Valid FINACK arrived!\n");
                            printf("Packet info - Type: %d\tSeq: %d\n", FINACK_packet->flags, FINACK_packet->seq);                            
                            
                            /* Finalize ACK_packet */
                            ACK_packet->seq = FINACK_packet->seq + 1;
                            ACK_packet->checksum = checksum(ACK_packet);

                            /* Switch to previous state */
                            s_state = RCVD_FINACK;

                        } else{ /* Invalid packet type or checksum */
                            printf("Invalid FINACK packet\n");
                            printf("Retransmitting FIN\n");

                            /* Switch to begining state */
                            s_state = ESTABLISHED;
                        }

                    } else{ /* Can't read from socket */
                        perror("Can't read socket");
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            /* Timeout occurred, close connection */
            case CLOSED:
                
                printf("Connection successfully closed!\n");

                /* Free allocated memory */
                free(FIN_packet);
                free(FINACK_packet);
                free(ACK_packet);
                
                return 1;   /* Return that connection was closed */
                break;

            default:
                break;
        }

    }

}

uint16_t checksum(rtp *packet){
    /* Calculate the number of 16-bit words in the packet */
    int nwords = (sizeof(packet->flags) + sizeof(packet->seq) + sizeof(packet->data))/sizeof(uint16_t);
    
    /* Create an array to hold the data for checksum calculation */
    uint16_t buffer_array[nwords];

    /* Combine seq and flags fields of the packet into the first element of the buffer_array */
    buffer_array[0] = (uint16_t)packet->seq + ((uint16_t)packet->flags << 8);

    /* Loop through each byte of the data field */
    for(int byte_index = 1; byte_index <= sizeof(packet->data); byte_index++){

        /* Calculate the index of the word where the byte will be stored */
        int word_index = (byte_index + 1) / 2;

        if(byte_index % 2 == 1){
            /* If byte_index is odd, store the byte in the lower byte of the word */
            buffer_array[word_index] = packet->data[byte_index - 1];
        } else{
            /* If byte_index is even, shift the previously stored byte to the higher byte
             * of the word and add the new byte */
            buffer_array[word_index] = buffer_array[word_index] << 8;
            buffer_array[word_index] += packet->data[byte_index - 1]; 
        }
    }

    /* create a pointer to buffer_array and initialize a 32-bit sum*/
    uint16_t *buffer = buffer_array;
    uint32_t sum;

    /*Iterate through each 16-bit word in buffer_array and sum them up*/
    for(sum = 0; nwords > 0; nwords--){
        sum += *buffer++;
    }

    /* Reduce the sum to a 16-bit value by adding the carry from the high
     * 16 bits to the low 16 bits */
    sum = (sum >> 16) + (sum & 0xffff);

    sum += (sum >>16);  /* Add any carry from the previous step */
    return ~sum;    /* Return the one's complement of the sum */

}









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
    int sockfd;                         /* Socket file descriptor of the sender */
   // int numRead;
    socklen_t socklen;                  /* Length of the socket structure sockaddr */
   // char buf[MAXMSG];
    struct sockaddr_in serverName;
    char hostName[hostNameLength];      /* Name of the host/receiver */

    socklen = sizeof(struct sockaddr);

    /*Check arguments*/
    if(argv[1] == NULL){
        perror("Usage: client [host name]\n");
        exit(EXIT_FAILURE);
    }
    else{
        strncpy(hostName, argv[1], hostNameLength);
        hostName[hostNameLength - 1] = '\0';
    }

    /* Create the socket */
    sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0){
        perror("Could not create a socket\n");
        exit(EXIT_FAILURE);
    }

    /* Initialize the socket address */
    initSocketAddress(&serverName, hostName, PORT);

    /* Start a connection to the server */
    if(sender_connection(sockfd, (struct sockaddr*)&serverName, socklen) != 1){
        perror("sender connection");
        exit(EXIT_FAILURE);
    }


    /* Send all packet 
    while(1){
        if(sender_gbn(sockfd, buf, numRead, 0) == -1){
            perror("sender_gbn");
            exit(EXIT_FAILURE);
        }
    }
    */
    
    /* Close the socket */
    if (sender_teardown(sockfd, (struct sockaddr*)&serverName, socklen) == -1){
        perror("teardown");
        exit(EXIT_FAILURE);
    }

    return 0;
}

/* File: server.c
 * Authors: Kim Svedberg, Zebastian Thorsén 
 * Description: File containing the receiver code.
 */

#include "GBN.h"

#define PORT 5555




receiver_connection(int sockfd, const struct sockaddr *client, socklen_t *socklen){
    fd_set activeFdSet;
    int nOfBytes = 0;
    int result = 0;
    r_state = LISTENING;

    /* Timeout */
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;


    /* Initilize SYN packet*/
    rtp *SYN_packet = malloc(sizeof(*SYN_packet));
    if(SYN_packet == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memset(SYN_packet->data, '\0', sizeof(SYN_packet->data));


    /* Initilize SYNACK packet */
    rtp *SYNACK_packet = malloc(sizeof(*SYNACK_packet));
    if(SYNACK_packet == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    SYNACK_packet->flags = SYNACK;
    memset(SYNACK_packet->data, '\0', sizeof(SYNACK_packet->data));


    /* Initilize ACK packet */
    rtp *ACK_packet = malloc(sizeof(*ACK_packet));
    if(ACK_packet == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }


    FD_ZERO(&activeFdSet);
    FD_SET(sockfd, &activeFdSet);

    while(1){
        switch(r_state){

            /* Listen for a vaild SYN packet */
            case LISTENING:
                printf("Current state: LISTENING\n");

                /* Check if a SYN packet has arrived */
                if(recvfrom(sockfd, SYN_packet, sizeof(*SYN_packet), 0, client, socklen) != -1){
                    printf("New packet arrived!\n");

                    /* Check if packet is vaild*/
                    if(SYN_packet->flags == SYN && SYN_packet->checksum == checksum(SYN_packet)){
                        printf("Valid SYN packet!\n");
                        printf("Packet info - Type: %d\tseq: %d\tWindowSize: %d\n", SYN_packet->flags, SYN_packet->seq, SYN_packet->windowsize);

                        /* Finilize SYNACK packet */
                        SYNACK_packet->seq = SYN_packet->seq + 1;
                        SYNACK_packet->windowsize = SYN_packet->windowsize;
                        SYNACK_packet->checksum = checksum(SYNACK_packet);

                        /* Switch to next state */
                        r_state = RCVD_SYN;

                    } else{ /* Packet is not vaild, not SYN or wrong checksum */
                        printf("Invalid SYN packet!\n");
                        
                        /* Continue to listen for SYN*/
                        r_state = LISTENING;
                    }
                } else{
                    perror("Can't receive SYN packet");
                    exit(EXIT_FAILURE);
                }
                break;

            /* Send a SYNACK after a vaild SYN packet*/
            case RCVD_SYN:
                
                /* Send back a SYNACK to sender */
                nOfBytes = sendto(sockfd,SYNACK_packet, sizeof(*SYNACK_packet), 0, client, *socklen);
                
                /* Failed to send SYNACK to sender */
                if(nOfBytes < 0){
                    perror("Can't send SYNACK packet");
                    exit(EXIT_FAILURE);

                } else{ /* Successfully sent SYNACK */
                    printf("SYNACK sent!\n");
                    r_state = WAIT_ACK;
                }
                break;

            case WAIT_ACK:

                timeout.tv_sec = 5;
                timeout.tv_usec = 0;

                result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);

                if(result == -1){   /* Select failed */
                    perror("Select failed");
                    exit(EXIT_FAILURE);

                } else if(result == 0){ /* timeout occurs, SYNACK packet lost */
                    printf("TIMEOUT: SYNACK packet lost\n");
                    r_state = RCVD_SYN;

                } else{ /* Receives a packet */
                    if(recvfrom(sockfd, ACK_packet, sizeof(*ACK_packet), 0, client, socklen) != -1){
                        printf("New packet arrived!\n");
                        

                        /* If the packet is a ACK and have a valid checksum */
                        if(ACK_packet->flags == ACK && ACK_packet->checksum == checksum(ACK_packet)){
                            printf("Valid ACK packet!\n");
                            printf("Packet info - Type: %d\tseq: %d\n", ACK_packet->flags, ACK_packet->seq);
                            
                            /* Switch to next state */
                            r_state = ESTABLISHED;

                        } else{ /* Invalid packet */
                            printf("Invalid ACK packet\n");

                            /* Switch to previous state */
                            r_state = RCVD_SYN;
                        }
                    } else{
                        perror("Can't read from socket");
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case ESTABLISHED:
                printf("Connection successfully established!\n\n");

                /* Free allocated memory */
                free(SYN_packet);
                free(SYNACK_packet);
                free(ACK_packet);

                return sockfd; /* Retrun that the connecton was made*/
                break;

            default:
                break;
        }
    }
    return -1;
}

int receiver_teardown(int sockfd, const struct sockaddr *client, socklen_t socklen){
    fd_set activeFdSet;
    int nOfBytes;
    int result;

    r_state = ESTABLISHED;

    /* Timeout */
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    /* Initilize FIN packet */
    rtp *FIN_packet = malloc(sizeof(*FIN_packet));
    memset(FIN_packet->data, '\0', sizeof(FIN_packet->data));
  
    /* Initilize FINACK packet */
    rtp *FINACK_packet = malloc(sizeof(*FINACK_packet));
    FINACK_packet->flags = FINACK;
    memset(FINACK_packet->data, '\0', sizeof(FINACK_packet));

    /* Initilize ACK packet */
    rtp *ACK_packet = malloc(sizeof(*ACK_packet));
    memset(ACK_packet->data, '\0', sizeof(ACK_packet));

    FD_ZERO(&activeFdSet);
    FD_SET(sockfd, &activeFdSet);

    while(1){
        switch(r_state){
            
            /* FIN received */
            case ESTABLISHED:
                if(recvfrom(sockfd, FIN_packet, sizeof(*FIN_packet), 0, client, &socklen) != -1){
                    
                    printf("New packet arrived!\n");

                    /* Check if packet is FIN and have a valid checksum */
                    if(FIN_packet->flags == FIN && FIN_packet->checksum == checksum(FIN_packet)){
                        printf("Valid FIN packet!\n");
                        printf("Packet info - Type: %d\tseq: %d\n", FIN_packet->flags, FIN_packet->seq);

                        /* Finilize FINACK packet */
                        FINACK_packet->seq = FIN_packet->seq + 1;
                        FINACK_packet->checksum = checksum(FINACK_packet);

                        /* Switch to next state */
                        r_state = RCVD_FIN;

                    } else{
                        printf("Invalid FIN packet!\n");

                        /* Stay in current state */
                        r_state = ESTABLISHED;
                    }

                } else{
                    perror("Can't read from socket");
                    exit(EXIT_FAILURE);
                }
                break;

            /* Received FIN, send FINACK */
            case RCVD_FIN:
                nOfBytes = sendto(sockfd, FINACK_packet, sizeof(*FINACK_packet), 0, client, socklen);
                
                /* Failed to send FINACK */
                if(nOfBytes < 0){
                    perror("Send FINACK failed");
                    exit(EXIT_FAILURE);
                } 

                printf("FINACK sent!\n");

                /* Switch to next state */
                r_state = WAIT_TIME;
                break;

            /* Sent FINACK, waiting if another packet arrives */
            case WAIT_TIME:

                timeout.tv_sec = 5;
                timeout.tv_usec = 0;

                result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);

                if(result == -1){
                    perror("Select failed");
                    exit(EXIT_FAILURE);

                } else if(result == 0){ /* Timeout occurs, FINACK was lost */
                    printf("TIMEOUT occurred\n");

                    /* Retransmit FINACK, switch to previous state */
                    r_state = RCVD_FIN;
                } else{
                    if(recvfrom(sockfd, ACK_packet, sizeof(*ACK_packet), 0, client, &socklen) != -1){
                        printf("New packet arrived!\n");

                        /* valid ACK */
                        if(ACK_packet->flags == ACK && ACK_packet->checksum == checksum(ACK_packet)){
                            printf("Valid ACK packet!\n");
                            printf("Packet info - Type: %d\tseq: %d\n", ACK_packet->flags, ACK_packet->seq);

                            /* Switch to next state */
                            r_state = CLOSED;

                        } else{
                            printf("Invalid ACK packet!\n");
                            
                            /* Switch to previous state */
                            r_state = RCVD_FIN;
                        }
                    } else{
                        perror("Can't read from socket");
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            /* Received ACK, close connection */
            case CLOSED:

                printf("Connection successfully closed!\n");

                /* Free allocated memory */
                free(FIN_packet);
                free(FINACK_packet);
                free(ACK_packet);

                return 1; /* Return that connection was closed */
                break;
            
            default:
                break;
        }
    }
}

/* Checksum calculator */
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
   // int numRead;                    /* Number of read bytes */
    //char buf[MAXMSG];
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

    /* Read all packets
    while(1){
        if((numRead = receiver_gbn(new_sockfd, buf, MAXMSG, 0)) == -1){
            perror("receiver_gbn");
            exit(EXIT_FAILURE);

        } else if(numRead == 0){
            break;
        }

    }
    */
    
    /* Close the socket */
    if(receiver_teardown(new_sockfd, (struct sockaddr*)&clientName, socklen) == -1){
        perror("receiver_teardown");
        exit(EXIT_FAILURE);
    }

    return 0;
}

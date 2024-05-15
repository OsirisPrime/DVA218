/* File: server.c
 * Authors: Kim Svedberg, Zebastian Thorsén 
 * Description: File containing the receiver code.
 */

#include "GBN.h"

#define PORT 5555


state_t r;


receiver_connection(int sockfd, const struct sockaddr *client, socklen_t *socklen){
    
    struct timeval timeout;                                 /* Timeout struct */
    fd_set activeFdSet;                                     /* File descriptor */
    int result = 0;                                         /* Select result */


    /* Initilize SYN packet*/
    rtp *SYN_packet = malloc(sizeof(*SYN_packet));          /* Alloc memory for SYN packet */
    if(SYN_packet == NULL) {                                /* Check if allocation failed */
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memset(SYN_packet->data, '\0', sizeof(SYN_packet->data));   /* Make data field empty */


    /* Initilize SYNACK packet */
    rtp *SYNACK_packet = malloc(sizeof(*SYNACK_packet));    /* Alloc memory for SYNACK packet */
    if(SYNACK_packet == NULL) {                             /* Check if allocation failed */
        perror("malloc");
        exit(EXIT_FAILURE);
    }


    /* Initilize ACK packet */
    rtp *ACK_packet = malloc(sizeof(*ACK_packet));          /* Alloc memory for ACK packet */
    if(ACK_packet == NULL) {                                /* Check if allocation failed */
        perror("malloc");
        exit(EXIT_FAILURE);
    }


    FD_ZERO(&activeFdSet);                                  /* Clear fd set */  
    FD_SET(sockfd, &activeFdSet);                           /* Add fd to set */

    r.state = LISTENING;                                    /* The start state */
    printf("STATE: LISTENING\n");

    /* State machine */
    while(1){
        switch(r.state){

            /* Listen for a vaild SYN packet */
            case LISTENING:

                /* Read from socket */
                if(recvfrom(sockfd, SYN_packet, sizeof(*SYN_packet), 0, client, socklen) != -1){
                    printf("New packet arrived!\n");

                    /* Packet is valid */
                    if(SYN_packet->flags == SYN && SYN_packet->checksum == checksum(SYN_packet)){
                        printf("Valid SYN!\n");
                        printf("Packet info: Type = %d\tSeq = %d\tWindowsize = %d\n\n", SYN_packet->flags, SYN_packet->seq, SYN_packet->windowsize);

                        /* Update receiver info */
                        r.seqnum = SYN_packet->seq;
                        r.window_size = SYN_packet->windowsize;

                        /* Finilize SYNACK packet */

                        /* Switch to next state */
                        r.state = RCVD_SYN;

                    /* Packet is invalid */
                    } else{
                        printf("Invalid SYN! Continue to listen\n");   /* Stay in current state */
                    }

                /* Failed to read from socket */
                } else{
                    perror(" Can't read from socket");
                    exit(EXIT_FAILURE);
                }
                break;

            /* Send a SYNACK after a vaild SYN packet*/
            case RCVD_SYN:
                
                SYNACK_packet->flags = SYNACK;                          /* Set flag to SYNACK */
                memset(SYNACK_packet->data, '\0', sizeof(SYNACK_packet->data)); /* Make data field empty */   
                SYNACK_packet->seq = r.seqnum;                          /* Set seqnum */
                SYNACK_packet->windowsize = SYN_packet->windowsize;     /* Set windowsize */
                SYNACK_packet->checksum = checksum(SYNACK_packet);      /* Calculate checksum*/

                /* Send SYNACK packet */
                if(maybe_sendto(sockfd, SYNACK_packet, sizeof(*SYNACK_packet), 0, client, *socklen) != -1){
                    printf("SYNACK sent: Type = %d\tSeq = %d\n\n", SYNACK_packet->flags, SYNACK_packet->seq);

                    /* Successfully sent SYNACK, switch to next state */
                    r.state = WAIT_ACK;

                /* Failed to send SYNACK */
                } else{
                    perror("Send SYNACK");
                    exit(EXIT_FAILURE);
                }
                break;

            case WAIT_ACK:

                /* Timeout var. */
                timeout.tv_sec = 0;                     /* 0s */
                timeout.tv_usec = 500;                  /* 500ms */

                result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);

                /* Select fails */
                if(result == -1){
                    perror("Select failed");
                    exit(EXIT_FAILURE);

                /* Timeout occurs, SYNACK packet lost */
                } else if(result == 0){
                    printf("TIMEOUT: SYNACK packet lost\n");
                    r.state = RCVD_SYN;             /* Switch to previous state */

                /* Packet arrived within time frame */
                } else{
                    
                    /* Read from socket */
                    if(recvfrom(sockfd, ACK_packet, sizeof(*ACK_packet), 0, client, socklen) != -1){
                        printf("New packet arrived!\n");

                        /* Packet is valid */
                        if(ACK_packet->flags == ACK && ACK_packet->seq == r.seqnum + 1 && ACK_packet->checksum == checksum(ACK_packet)){
                            printf("Valid ACK!\n");
                            printf("Packet info: Type = %d\tSeq = %d\n\n", ACK_packet->flags, ACK_packet->seq);

                            /* Update receiver info */
                            r.seqnum = ACK_packet->seq;

                            /* Switch to next state */
                            r.state = ESTABLISHED;

                        /* Packet is invalid */    
                        } else {
                            printf("Invalid ACK packet. Retransmitting SYNACK\n");

                            /*Switch to previous state */
                            r.state = RCVD_SYN;
                        }

                    /* Failed to read from socket */
                    } else{
                        perror("Can't read from socket");
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case ESTABLISHED:
                printf("Connection established!\n\n\n\n");

                /* Free allocated memory */
                free(SYN_packet);
                free(SYNACK_packet);
                free(ACK_packet);
                return sockfd;              /* Return socket */
                break;

            default:
                break;
        }
    }
    return -1;
}

int receiver_teardown(int sockfd, const struct sockaddr *client, socklen_t socklen){
    
    struct timeval timeout;                                 /* Timeout struct */
    fd_set activeFdSet;                                     /* File descriptor */
    int result = 0;                                         /* Select result */


    /* Initilize FIN packet */
    rtp *FIN_packet = malloc(sizeof(*FIN_packet));          /* Alloc memory for FIN packet */      
    if(FIN_packet == NULL) {                                /* Check if allocation failed */
        perror("malloc");
        exit(EXIT_FAILURE);
    }    
    memset(FIN_packet->data, '\0', sizeof(FIN_packet->data));   /* Make data field empty */
  

    /* Initilize FINACK packet */
    rtp *FINACK_packet = malloc(sizeof(*FINACK_packet));    /* Alloc memory from FINACK packet */
    if(FINACK_packet == NULL) {                                /* Check if allocation failed */
        perror("malloc");
        exit(EXIT_FAILURE);
    } 


    /* Initilize ACK packet */
    rtp *ACK_packet = malloc(sizeof(*ACK_packet));          /* Alloc memory for ACK packet */
    if(ACK_packet == NULL) {                                /* Check if allocation failed */
        perror("malloc");
        exit(EXIT_FAILURE);
    } 
    memset(ACK_packet->data, '\0', sizeof(ACK_packet));


    FD_ZERO(&activeFdSet);                                  /* Clear fd set */
    FD_SET(sockfd, &activeFdSet);                           /* Add fd to set */

    /* State machine */
    while(1){
        switch(r.state){
            
            /* FIN received */
            case ESTABLISHED:

                /* Read from socket */
                if(recvfrom(sockfd, FIN_packet, sizeof(*FIN_packet), 0, client, &socklen) != -1){   
                    printf("New packet arrived!\n");

                    /* Packet is valid  */
                    if(FIN_packet->flags == FIN && FIN_packet->seq == r.seqnum + 1 && FIN_packet->checksum == checksum(FIN_packet)){
                        printf("Valid FIN!\n");
                        printf("Packet info: Type = %d\tSeq = %d\n\n", FIN_packet->flags, FIN_packet->seq);
                        
                        /* Update receiver info */
                        r.seqnum++;

                        /* Finilize FINACK packet */
                        FINACK_packet->flags = FINACK;                      /*Set flag to FINACK */
                        FINACK_packet->seq = r.seqnum;                      /* Set seqnum */                   
                        memset(FINACK_packet->data, '\0', sizeof(FINACK_packet));   /* Make data field empty */
                        FINACK_packet->checksum = checksum(FINACK_packet);  /* Calculate checksum */

                        /* Switch to next state */
                        r.state = RCVD_FIN;

                    /* Packet is invalid */
                    } else{
                        printf("Invalid FIN packet!\n");

                        /* Stay in current state */
                        r.state = ESTABLISHED;
                    }

                /* Failed to read from socket */
                } else{
                    perror("Can't read from socket");
                    exit(EXIT_FAILURE);
                }
                break;

            /* Received FIN, send FINACK */
            case RCVD_FIN:

                /* Send FINACK */
                if(maybe_sendto(sockfd, FINACK_packet, sizeof(*FINACK_packet), 0, client, socklen) != -1){
                    printf("FINACK sent: Type = %d\tSeq = %d\n\n", FINACK_packet->flags, FINACK_packet->seq);

                    /* Switch to next state */
                    r.state = WAIT_TIME;
                
                /* Failed to send FINACK */
                } else {
                    perror("Send FINACK");
                    exit(EXIT_FAILURE);                    
                }
                break;

            /* Sent FINACK, waiting if another packet arrives */
            case WAIT_TIME:

                /* Timeout var. */
                timeout.tv_sec = 5;                 /* 5s */
                timeout.tv_usec = 0;                /* 0ms */

                result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);

                /* Select fails */
                if(result == -1){
                    perror("Select failed");
                    exit(EXIT_FAILURE);
                
                /* Timeout occurs, FINACK was lost */
                } else if(result == 0){ 
                    printf("TIMEOUT occurred\n");

                    /* Retransmit FINACK, switch to previous state */
                    r.state = RCVD_FIN;

                /* Packet arrived within time frame */    
                } else{

                    /* Read from socket */
                    if(recvfrom(sockfd, ACK_packet, sizeof(*ACK_packet), 0, client, &socklen) != -1){
                        printf("New packet arrived!\n");

                        /* packet is valid */
                        if(ACK_packet->flags == ACK && ACK_packet->seq == r.seqnum + 1 && ACK_packet->checksum == checksum(ACK_packet)){
                            printf("Valid ACK!\n");
                            printf("Packet info: Type = %d\tSeq = %d\n\n", ACK_packet->flags, ACK_packet->seq);

                            /* Update receiver info */
                            r.seqnum++;

                            /* Switch to next state */
                            r.state = CLOSED;

                        /* Packet is invalid */
                        } else{
                            printf("Invalid ACK packet!\n");
                            
                            /* Switch to previous state */
                            r.state = RCVD_FIN;
                        }

                    /* Failed to read from socket */
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
                return 1;               /* Return that connection was closed */
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

/* ERROR generator */
ssize_t maybe_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen){
    char *buffer = malloc(len);
    memcpy(buffer, buf, len);

    /* Packet not lost */
    if(rand() > LOSS_PROB * RAND_MAX){

        /* Packet corrupted */
        if(rand() < CORR_PROB * RAND_MAX){

            /* Selecting a random byte inside the packet */
            int index = (int)((len - 1)*rand()/(RAND_MAX + 1.0));

            /* Inverting a bit */
            char c = buffer[index];
            if(c & 0x01){
                c &= 0xFE;
            } else{
                c |= 0x01;
            }
            buffer[index] = c;
        }

        /* Sending the packet */
        int result = sendto(sockfd, buffer, len, flags, to, tolen);
        if(result == -1){
            perror("maybe_sendto problem");
            exit(EXIT_FAILURE);
        }
        /* Free buffer and return the bytes sent */
        free(buffer);
        return result;

    } else{ /* Packet lost */
        return(len);
    }
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

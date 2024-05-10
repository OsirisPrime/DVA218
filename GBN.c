#include "GBN.h"


int sender_connection(int sockfd, const struct sockaddr *serverName){
    char buffer[MAXMSG];
    fd_set activeFdSet;
    srand(time(NULL));
    int nOfBytes;

    s_state = CLOSED;

    /* Timeout */
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100;
    
    /* Initialize SYN_packet */
    rtp *SYN_packet = malloc(sizeof(*SYN_packet));
    SYN_packet->flags = SYN;    //SYN packet                     
    SYN_packet->seq = (rand() % 10000) + 10;    //Chose a random seq_number between 10 & 9999
    SYN_packet->windowsize = windowSize;
    memset(SYN_packet->data, '\0', sizeof(SYN_packet));
    SYN_packet->checksum = checksum(SYN_packet);

    /* Initilize SYNACK_packet */
    rtp *SYNACK_packet = malloc(sizeof(*SYNACK_packet));
    memset(SYNACK_packet->data, '\0', sizeof(SYNACK_packet->data));
    struct sockaddr from;
    socklen_t from_len = sizeof(from);

    /* Initialize ACK_packet */
    rtp *ACK_packet = malloc(sizeof(*ACK_packet));
    ACK_packet->flags = ACK;
    memset(ACK_packet->data, '\0', sizeof(ACK_packet->data));


    FD_Zero(&activeFdSet);
    FD_SET(sockfd, &activeFdSet);


    while(1){
        switch(s_state){
            case CLOSED:

                printf("Sensing SYN_packet\n");

                nOfBytes = sendto(sockfd, &SYN_packet, sizeof(*SYN_packet), 0, serverName, sizeof(serverName));
            
                if(nOfBytes < 0){
                perror("Sender connection - Could not send SYN packet\n");
                exit(EXIT_FAILURE);
                }

                s_state = WAIT_SYNACK;
                break;
            
            case WAIT_SYNACK:
                int result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);

                if(result == -1){   //Select fails
                    perror("WAIT_SYN select failed");
                    exit(EXIT_FAILURE);

                } else if(result == 0){ //If the a timeout occurs, packet loss
                    printf("TIMEOUT: SYN packet loss\n");
                    s_state = CLOSED;
                    break;

                } else{ //Recieves SYNACK
                    if(recvfrom(sockfd, SYNACK_packet, sizeof(*SYNACK_packet), 0, &from, &from_len) != -1){
                        printf("SYNACK received!\n");
                        printf("Packet info - Type: %d\tSeq: %d\n", SYNACK_packet->flags, SYNACK_packet->seq);

                        /* If the packet is a SYNACK and have a valid checksum*/
                        if(SYNACK_packet->flags == SYNACK && SYNACK_packet->seq == SYN_packet->seq + 1 &&SYNACK_packet->checksum == checksum(SYNACK_packet)){
                            printf("Checksum match, valid SYNACK!\n");
                            s_state = RCVD_SYNACK;

                            /**/
                            ACK_packet->seq = SYNACK_packet->seq + 1;
                            ACK_packet->checksum = checksum(ACK_packet);

                        } else{ //If the packet is not a SYNACK or mismatch of checksum
                            printf("Invalid SYNACK\n");
                            s_state = CLOSED;
                        }
                    } else{
                        perror("Can't read socket");
                        exit(EXIT_FAILURE);
                    }  
                }
                break;

            case RCVD_SYNACK:
                nOfBytes = sendto(sockfd, &ACK_packet, sizeof(*ACK_packet), 0, serverName, sizeof(serverName));
                
                if(nOfBytes < 0){
                perror("Sender connection - Could not send SYN packet\n");
                exit(EXIT_FAILURE);
                }

                s_state = ESTABLISHED;
                break;

            case ESTABLISHED:
                int result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);

                if(result == -1){   //Select fails
                    perror("ESTABLISHED select failed");
                    exit(EXIT_FAILURE);

                } else if(result == 0){ //Timeout occurs, no packet loss
                    printf("Connection succeccfully established\n");
                    free(SYN_packet);
                    free(SYNACK_packet);
                    free(ACK_packet);
                    return 1;

                } else{ //Receives SYNACK again, ACK packet was lost
                    printf("SYNACK received again\n");
                    s_state = RCVD_SYNACK;
                }
                break;

            default:
                break;
        }
    }
}


int sender_teardown(int sockfd, struct sockaddr_in *serverName){
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    fd_set activeFdSet;
    srand(time(NULL));
    int nOfBytes;

    s_state = ESTABLISHED;

    /* Timeout */
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100;

    /* FIN packet */
    rtp *FIN_packet = malloc(sizeof(*FIN_packet));
    FIN_packet->flags = FIN;
    memset(FIN_packet->data, '\0', sizeof(FIN_packet->data));
    FIN_packet->checksum = checksum(FIN_packet);

    /* FINACK packet */
    rtp *FINACK_packet = malloc(sizeof(*FINACK_packet));
    FINACK_packet->flags = FINACK;
    memset(FINACK_packet->data, '\0', sizeof(FINACK_packet));

    /* ACK packet */
    rtp *ACK_packet = malloc(sizeof(*ACK_packet));
    ACK_packet->flags = ACK;
    memset(ACK_packet->data, '\0', sizeof(ACK_packet));


    FD_Zero(&activeFdSet);
    FD_SET(sockfd, &activeFdSet);


    while(1){
        switch (s_state){

            case ESTABLISHED:   /* Start state */
                nOfBytes = sendto(sockfd, FIN_packet, sizeof(*FIN_packet), 0, serverName, sizeof(serverName));
                if(nOfBytes == -1){
                    perror("Send FIN_packet fail\n");
                    exit(EXIT_FAILURE);
                }
                s_state = WAIT_FINACK;
                break;

            case WAIT_FINACK:   /* Sent FIN packet */
                int result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);
                
                if(result == -1){   /* Select failed */
                    perror("select failed");
                    exit(EXIT_FAILURE);

                } else if(result == 0){ /* Timeout occurs, packet lost */
                    printf("TIMEOUT - FIN packet loss\n");
                    s_state = ESTABLISHED;

                } else{ /* Receives a packet */
                    /* Can read from socket */
                    if(recvfrom(sockfd, FINACK_packet, sizeof(*FINACK_packet), 0, &from, &fromlen) != -1){
                        printf("FINACK recieved!\n");
                        printf("Packet info - Type: %d\tSeq: %d\n", FINACK_packet->flags, FINACK_packet->seq);

                        /* Correct packet type and checksum */
                        if(FINACK_packet->flags == FINACK && FINACK_packet->checksum == checksum(FINACK_packet)){
                            s_state = RCVD_FINACK;

                            ACK_packet->seq = FINACK_packet->seq + 1;
                            ACK_packet->checksum = checksum(ACK_packet);

                        } else{ /* Invalid packet type or checksum */
                            printf("Invalid FINACK\n");
                            s_state = ESTABLISHED;
                        }

                    } else{ /* Can't read from socket */
                        perror("Can't read socket");
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case RCVD_FINACK:
                int result = sendto(sockfd, ACK_packet, sizeof(*ACK_packet), 0, serverName, sizeof(serverName));
                if(result == -1){
                    perror("Send ACK-packet fail");
                    exit(EXIT_FAILURE);
                }
                s_state = WAIT_TIME;
                break;

            case WAIT_TIME:
                int result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);
                
                if(result == -1){   /* Select failed */
                    perror("select failed");
                    exit(EXIT_FAILURE);

                } else if(result == 0){ /* Timeout occurs, packet arrived */
                    printf("TIMEOUT - connection closed\n");
                    s_state = CLOSED;

                    free(FIN_packet);
                    free(FINACK_packet);
                    free(ACK_packet);
                    return 1;

                } else{ /* Receives a new packet */
                    printf("Packet arrived again!\n");
                    
                    if(recvfrom(sockfd, FINACK_packet, sizeof(*FINACK_packet), 0, &from, &fromlen) != -1){
                        printf("FINACK recieved!\n");
                        printf("Packet info - Type: %d\tSeq: %d\n", FINACK_packet->flags, FINACK_packet->seq);

                        /* Correct packet type and checksum */
                        if(FINACK_packet->flags == FINACK && FINACK_packet->checksum == checksum(FINACK_packet)){
                            s_state = RCVD_FINACK;

                            ACK_packet->seq = FINACK_packet->seq + 1;
                            ACK_packet->checksum = checksum(ACK_packet);

                        } else{ /* Invalid packet type or checksum */
                            printf("Invalid FINACK\n");
                            s_state = ESTABLISHED;
                        }

                    } else{ /* Can't read from socket */
                        perror("Can't read socket");
                        exit(EXIT_FAILURE);
                    }
                }
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

/* ERROR generator*/
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
        free(buffer);
        return result;

    } else{ /* Packet lost */
        return(len);
    }
}

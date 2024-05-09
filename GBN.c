#include "GBN.h"


int sender_connection(int sockfd, const struct sockaddr *serverName){
    char buffer[MAXMSG];
    fd_set activeFdSet;
    srand(time(NULL));
    int nOfBytes;

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100;

    s_state = CLOSED;
    
    /* Initialize SYN_packet */
    rtp *SYN_packet = malloc(sizeof(*SYN_packet));
    SYN_packet->flags = SYN;    //SYN packet                     
    SYN_packet->seq = (rand() % 10000) + 10;    //Chose a random seq_number between 10 & 9999
    SYN_packet->windowsize = windowSize;
    memset(SYN_packet->data, '\0', sizeof(SYN_packet));
    SYN_packet->checksum = checksum(SYN_packet);

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
                    printf("TIMEOUT: SYN packet loss");
                    s_state = CLOSED;
                    break;

                } else{ //Recieves SYNACK
                    printf("SYNACK received");
                    s_state = RCVD_SYNACK;
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
                    printf("Connection succeccfully established");
                    return 1;

                } else{ //Receives SYNACK again, ACK packet was lost
                    printf("SYNACK received again");
                    s_state = RCVD_SYNACK;
                }

                break;
        }
    }
}


int teardown(int sock, struct sockaddr_in serverName){

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

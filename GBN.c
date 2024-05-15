/* File: GBN.c
 * Authors: Kim Svedberg, Zebastian Thorsén 
 * Description: File containing all function for the GBN protocol like connection setup & teardown. 
 */

#include "GBN.h"

state_t s, r;

int sender_connection(int sockfd, const struct sockaddr *serverName, socklen_t socklen){
    
    struct timeval timeout;                                 /* Timeout struct  */
    fd_set activeFdSet;                                     /* File descriptor */
    srand(time(NULL));                                      /* Random number generator */
    int result = 0;                                         /* Select result */
    
    struct sockaddr from;                                   /* Adress from receiver */           
    socklen_t from_len = sizeof(from);                      /* socket length from receiver */    
    s.seqnum = (rand() % (MAX_SEQ - MIN_SEQ + 1))+ MIN_SEQ; /* Get a random seq number */ 

    /* Initialize SYN packet */
    rtp *SYN_packet = malloc(sizeof(*SYN_packet));          /* Alloc memory for SYN packet */
    if(SYN_packet == NULL) {                                /* Check if allocating memory failed */
        perror("malloc");
        exit(EXIT_FAILURE);
    }


    /* Initilize SYNACK packet */
    rtp *SYNACK_packet = malloc(sizeof(*SYNACK_packet));    /* Alloc memory for SYNACK packet */
    if(SYNACK_packet == NULL) {                             /* Check if allocating memory failed */
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memset(SYNACK_packet->data, '\0', sizeof(SYNACK_packet->data)); /* Make empty data field */
    

    /* Initialize ACK packet */
    rtp *ACK_packet = malloc(sizeof(*ACK_packet));          /* Alloc memory for ACK packet */
    if(ACK_packet == NULL) {                                /* Check if allocationg memory failed */
        perror("malloc");
        exit(EXIT_FAILURE);
    }


    FD_ZERO(&activeFdSet);                                  /* Clear fd set */
    FD_SET(sockfd, &activeFdSet);                           /* Add fd to set */

    s.state = CLOSED;                                       /* The start state */
    printf("STATE: CLOSED\n");

    /* State machine */
    while(1){
        switch(s.state){

            /* Connection closed. Send SYN */
            case CLOSED:

                SYN_packet->flags = SYN;                                /* Set flag to SYN */  
                SYN_packet->seq = s.seqnum;                             /* Set start seq to the random seq*/                                   
                SYN_packet->windowsize = windowSize;                    /* Set windowsize */
                memset(SYN_packet->data, '\0', sizeof(SYN_packet->data));   /* Make empty data field */  
                SYN_packet->checksum = checksum(SYN_packet);            /* Calculate checksum */
            
                /* Send SYN packet */
                if(maybe_sendto(sockfd, SYN_packet, sizeof(*SYN_packet), 0, serverName, socklen) != -1){
                    printf("SYN sent: Type = %d\tSeq = %d\tWindowsize = %d\n\n", SYN_packet->flags, SYN_packet->seq, SYN_packet->windowsize);

                    /* Successfully sent SYN, switch to next state */
                    s.state = WAIT_SYNACK;

                /* Failed to send SYN */
                } else{
                    perror("Send SYN");
                    exit(EXIT_FAILURE);
                }
                break;
         
            /* Wait for a SYNACK for the SYN */
            case WAIT_SYNACK:

                /* Timeout var. */
                timeout.tv_sec = 2;                 /* 2s */
                timeout.tv_usec = 0;                /* 0ms*/

                /* Look if a packet has arrived */
                result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);
                
                /* Select fails */
                if(result == -1){                   
                    perror("Select failed");
                    exit(EXIT_FAILURE);
                
                /* Timeout occurs */
                } else if(result == 0){             
                    printf("TIMEOUT: SYN packet lost\n");
                    s.state = CLOSED;

                /* Packet received within time frame */
                } else{   

                    /* Read from socket */  
                    if(recvfrom(sockfd, SYNACK_packet, sizeof(*SYNACK_packet), 0, &from, &from_len) != -1){
                        printf("New packet arrived!\n");

                        /* Packet is valid */
                        if(SYNACK_packet->flags == SYNACK && SYNACK_packet->seq == s.seqnum && SYNACK_packet->checksum == checksum(SYNACK_packet)){
                            printf("Valid SYNACK!\n");
                            printf("Packet info: Type = %d\tSeq = %d\n\n", SYNACK_packet->flags, SYNACK_packet->seq);

                            /* Update sender info */
                            s.seqnum++;                                     /* Increase seqnum by one */
                            s.window_size = SYNACK_packet->windowsize;      /* Set agreed windowsize */

                            /* Switch to next state */
                            s.state = RCVD_SYNACK;

                        /* Packet is invalid */    
                        } else{
                            printf("Invalid SYNACK packet. Retransmitting SYN\n");
                            printf("Packet info: Type = %d\tSeq = %d\n\n", SYNACK_packet->flags, SYNACK_packet->seq);

                            /* Switch to previous state */
                            s.state = CLOSED;
                        }

                    /* Failed to read from socket */
                    } else{
                        perror("Can't read from socket");
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            /* Received SYNACK, send an ACK*/
            case RCVD_SYNACK:

                /* Finilize ACK packet */
                ACK_packet->flags = ACK;                                    /* Set flag to ACK */
                ACK_packet->seq = s.seqnum;                                 /* Set seqnum */                      
                memset(ACK_packet->data, '\0', sizeof(ACK_packet->data));   /* Make empty data field */
                ACK_packet->checksum = checksum(ACK_packet);                /* Calculate checksum */

                /* Send ACK packet */
                if(maybe_sendto(sockfd, ACK_packet, sizeof(*ACK_packet), 0, serverName, socklen) != -1){
                    printf("ACK sent: Type = %d\tSeq = %d\n\n", ACK_packet->flags, ACK_packet->seq);

                    /* Switch to next state */
                    s.state = ESTABLISHED;

                /* Failed to send ACK */
                } else{
                    perror("Send ACK");
                    exit(EXIT_FAILURE);
                }
                break;

            /* Sent an ACK for the SYNACK, wait for retransmissions */
            case ESTABLISHED:
                
                timeout.tv_sec = 2;                 /* 2s */
                timeout.tv_usec = 0;                /* 0ms */

                result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);

                /* Select fails */
                if(result == -1){
                    perror("Select failed");
                    exit(EXIT_FAILURE);

                /* Timeout occurs, no packet loss */
                } else if(result == 0){
                    printf("TIMEOUT occured! Connection established!\n\n\n\n");

                    /* Free all allocated memory */
                    free(SYN_packet);
                    free(SYNACK_packet);
                    free(ACK_packet);
                    return 1;           /* Return 1 */
                
                /* Receives new packet */
                } else{
                    if(recvfrom(sockfd, SYNACK_packet, sizeof(*SYNACK_packet), 0, &from, &from_len) != -1){
                        /* Resend ACK */
                        s.state = RCVD_SYNACK;

                    } else{
                        perror("Can't read from socket");
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            default:
                break;
        }
    }
}


int receiver_connection(int sockfd, const struct sockaddr *client, socklen_t *socklen){
    
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
                
                SYNACK_packet->flags = SYNACK;                              /* Set flag to SYNACK */
                SYNACK_packet->seq = r.seqnum;                              /* Set seqnum */
                SYNACK_packet->windowsize = r.window_size;                  /* Set windowsize */
                memset(SYNACK_packet->data, '\0', sizeof(SYNACK_packet->data)); /* Make data field empty */                
                SYNACK_packet->checksum = checksum(SYNACK_packet);          /* Calculate checksum*/

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
                            r.seqnum++;

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


int sender_teardown(int sockfd, const struct sockaddr *serverName, socklen_t socklen){

    struct timeval timeout;                                 /* Timeout struct */
    fd_set activeFdSet;                                     /* File descriptor */
    int result = 0;                                         /* Select result */

    struct sockaddr_in from;                                /* Socket adress */
    socklen_t fromlen = sizeof(from);                       /* Socket length */


    /* Initilize FIN packet */
    rtp *FIN_packet = malloc(sizeof(*FIN_packet));          /* Alloc memory for FIN packet */
    if(FIN_packet == NULL) {                                /* Check if allocating memory failed */
        perror("malloc");
        exit(EXIT_FAILURE);
    }


    /* Initilize FINACK packet */
    rtp *FINACK_packet = malloc(sizeof(*FINACK_packet));    /* Alloc memory for FINACK packet */        
    if(FINACK_packet == NULL) {                             /* Check if allocating memory failed */
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memset(FINACK_packet->data, '\0', sizeof(FINACK_packet->data));


    /* Initilize ACK packet */
    rtp *ACK_packet = malloc(sizeof(*ACK_packet));          /* Alloc memory for ACK packet */
    if(ACK_packet == NULL) {                                /* Check if allocating memory failed */
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    s.seqnum++;

    FD_ZERO(&activeFdSet);                                  /* Clear fd set */
    FD_SET(sockfd, &activeFdSet);                           /* Add fd to set */

    /* State machine */
    while(1){
        switch (s.state){

            /* Connection established. Send FIN */
            case ESTABLISHED:

                FIN_packet->flags = FIN;                        /* Set flag to FIN */
                FIN_packet->seq = s.seqnum;                     /* Set seqnum */
                memset(FIN_packet->data, '\0', sizeof(FIN_packet->data));   /* Make empty data field */
                FIN_packet->checksum = checksum(FIN_packet);    /* Calculate checksum */

                /* Send FIN packet */
                if(maybe_sendto(sockfd, FIN_packet, sizeof(*FIN_packet), 0, serverName, socklen) != -1){
                    printf("FIN sent: Type = %d\tSeq = %d\n\n", FIN_packet->flags, FIN_packet->seq);

                    /* Successfully sent FIN_packet, switch to next state */
                    s.state = WAIT_FINACK;

                /* Failed to send FIN */    
                } else{
                    perror("Send FIN");
                    exit(EXIT_FAILURE);
                }
                break;

            /* Wait for a FINACK for the FIN*/
            case WAIT_FINACK:

                /* Timeout var. */
                timeout.tv_sec = 1;                         /* 1s */
                timeout.tv_usec = 0;                        /* 0ms */

                /* Look if a packet has arrived */
                result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);
                
                /* Select failed */                
                if(result == -1){   
                    perror("select failed");
                    exit(EXIT_FAILURE);

                /* Timeout occurs */
                } else if(result == 0){ 
                    printf("TIMEOUT - FIN packet loss\n");
                    s.state = ESTABLISHED;

                /* Packet received within time frame */
                } else{

                    /* Read from socket */
                    if(recvfrom(sockfd, FINACK_packet, sizeof(*FINACK_packet), 0, &from, &fromlen) != -1){
                        printf("New packet arrived!\n");

                        /* Packet is valid */
                        if(FINACK_packet->flags == FINACK && FINACK_packet->seq == s.seqnum && FINACK_packet->checksum == checksum(FINACK_packet)){
                            printf("Valid FINACK!\n");
                            printf("Packet info: Type = %d\tSeq = %d\n\n", FINACK_packet->flags, FINACK_packet->seq);                            
                            
                            /* Update sender info */
                            s.seqnum++;

                            /* Switch to next state */
                            s.state = RCVD_FINACK;

                        /* Packet is invalid */
                        } else{
                            printf("Invalid FINACK. Retransmitting FIN\n");

                            /* Switch to previous state */
                            s.state = ESTABLISHED;
                        }
                    
                    /* Can't read from socket */
                    } else{ 
                        perror("Can't read socket");
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case RCVD_FINACK:

                /* Finalize the ACK packet */
                ACK_packet->flags = ACK;                                    /* Set flag to ACK */
                ACK_packet->seq = s.seqnum;                                 /* Set seqnum */
                memset(ACK_packet->data, '\0', sizeof(ACK_packet->data));   /* Make empty data field */
                ACK_packet->checksum = checksum(ACK_packet);                /* Calculate checksum */

                /* Send ACK packet */
                if(maybe_sendto(sockfd, ACK_packet, sizeof(*ACK_packet), 0, serverName, socklen) != -1){
                    printf("ACK sent: Type = %d\tSeq = %d\n\n", ACK_packet->flags, ACK_packet->seq);

                    /* Switch to next state */
                    s.state = WAIT_TIME;  

                /* Failed to send ACK*/                  
                } else{
                    perror("Send ACK");
                    exit(EXIT_FAILURE);                    
                }
                break;

            case WAIT_TIME:

                /* Timeout var. */
                timeout.tv_sec = 2;                 /* 2s */
                timeout.tv_usec = 0;                /* 0ms */

                /* Look if a packet has arrived */
                result = select(sockfd + 1, &activeFdSet, NULL, NULL, &timeout);
                
                /* Select failed */                
                if(result == -1){   
                    perror("select failed");
                    exit(EXIT_FAILURE);
                
                /* Timeout occurs, packet arrived */
                } else if(result == 0){ 
                    printf("TIMEOUT occurred! ");

                    /* Switch to next state */
                    s.state = CLOSED;
                
                /* Receives a new packet, ACK was lost */
                } else{ 
                    printf("Packet arrived again!\n");

                    /* Read from socket */
                    if(recvfrom(sockfd, FINACK_packet, sizeof(*FINACK_packet), 0, &from, &fromlen) != -1){
                            
                        /* Switch to previous state */
                        s.state = RCVD_FINACK;

                    /* FAiled to read from socket */                            
                    }else{
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

                /* Finilize FINACK packet */
                FINACK_packet->flags = FINACK;                          /*Set flag to FINACK */
                FINACK_packet->seq = r.seqnum;                          /* Set seqnum */                   
                memset(FINACK_packet->data, '\0', sizeof(FINACK_packet));   /* Make data field empty */
                FINACK_packet->checksum = checksum(FINACK_packet);      /* Calculate checksum */

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
                timeout.tv_sec = 1;                 /* 1s */
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


ssize_t sender_gbn(int sockfd, const void *buf, size_t len, int flags){

    /* Initialize DATA packet */
    rtp *DATA_packet = malloc(sizeof(*DATA_packet));
    DATA_packet->flags = DATA;
    memset(DATA_packet->data, '\0', sizeof(DATA_packet->data));

    /* Initialize ACK packet */
    rtp *ACK_packet =malloc(sizeof(*ACK_packet));
    memset(ACK_packet->data, '\0', sizeof(ACK_packet->data));

    struct sockaddr client_sockaddr;
    socklen_t client_socklen = sizeof(client_sockaddr);

    int num_UNACK_packet = 0;

    while(len > 0){
        switch(s_state){
            case ESTABLISHED:
                break;

            case SEND_DATA:
                break;

            case PACKET_LOSS:
                break;

            case RCVD_ACK:
                break;

            default:
                break;
        }
    }

    /* Free allocated memory */
    free(DATA_packet);
    free(ACK_packet);
    return 1;

}


ssize_t receiver_gbn(int sockfd, void *buf, size_t len, int flags){

    int expSeq;

    /* Initialize DATA packet */
    rtp *DATA_packet = malloc(sizeof(*DATA_packet));
    memset(DATA_packet->data, '\0', sizeof(DATA_packet->data));

    /* Initilaze ACK packet */
    rtp *ACK_packet =malloc(sizeof(*ACK_packet));
    ACK_packet->flags = ACK;
    memset(ACK_packet->data, '\0', sizeof(ACK_packet->data));

    struct sockaddr client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while(r_state == ESTABLISHED){
        if(recvfrom(sockfd, DATA_packet, sizeof(*DATA_packet), 0, &client_addr, &client_len) != -1){
            printf("Received a packet!\n");

            /* If the packet is a FIN */
            if(DATA_packet->flags == FIN && DATA_packet->checksum == checksum(DATA_packet)){
                printf("Received a valid FIN packet!\n");
                return 0;

            } else{ /* If the packet is not FIN*/
                if(DATA_packet->flags == DATA && DATA_packet->checksum == checksum(DATA_packet)){
                    printf("Received a valid DATA packet!\n");

                    /* If the data packet has expected sequence number */
                    if(DATA_packet->seq == expSeq){
                        printf("Data packet has expected sequence number!\n");
                        
                        /* Finilize ACK packet */
                        ACK_packet->seq = DATA_packet->seq + 1;
                        ACK_packet->checksum = checksum(ACK_packet);

                    } else{ /* wrong sequence number, resend old ACK*/
                        printf("DATA packet has wrong sequence number!\n");

                        /* Finialize ACK packet */
                        ACK_packet->seq = expSeq;
                        ACK_packet->checksum = checksum(ACK_packet);
                    }

                    /* Regardless of sequencxe number, send ACK */
                    if(maybe_sendto(sockfd, ACK_packet, sizeof(*ACK_packet), 0, &client_addr, &client_len) == -1){
                        perror("maybe_sendto");
                        exit(EXIT_FAILURE);

                    } else{
                        printf("Successfully sent ACK!\n");
                    }
                }
            }

        } else{
            r_state = CLOSED;
            perror("Can't read from socket\n");
            exit(EXIT_FAILURE);
        }
    }

    /* free allocated memory */
    free(DATA_packet);
    free(ACK_packet);
    return 0;
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

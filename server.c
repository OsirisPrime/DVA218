/*
==========================================================================
File            : client.c
Authurs         : Kim Svedberg & Zebastian Thorsén 
Version         : 1.0
Description     : A reliable GBN transfer  protocol built on top of UDP
==========================================================================
*/

#include "gbn.h"

/* Varr. for send & recv thread */
int base;
int nextSeq;
int expSeqNum;
state_t client;
pthread_mutex_t mutex;


/* Connection function. Send SYN, rcvd SYNACK and send ACK */
int sender_connect(int sockfd, const struct sockaddr *serverName, socklen_t socklen){
    rtp SYN_packet, SYNACK_packet, ACK_packet;          /* Defining packets */
    struct sockaddr from;                               /* Adress from receiver */           
    socklen_t from_len = sizeof(from);                  /* socket length from receiver */  

    SYN_packet = packetBuild(SYN_packet, SYN);          /* Build the SYN packet */

    /* Send the SYN packet to server */
    if(maybe_sendto(sockfd, &SYN_packet, sizeof(SYN_packet), 0, serverName, socklen) != -1){
        printf("SYN sent: Type = %d\tSeq = %d\tWindowsize = %d\n\n", SYN_packet.flags, SYN_packet.seq, SYN_packet.winSize);

    /* Failed to send SYN */
    } else{
        perror("Send SYN");
        exit(EXIT_FAILURE);
    }

    client.state = WAIT_SYNACK;                         /* Change client state */
    alarm(TIMEOUT);                                     /* Start a timer */

    while(1){

        /* Read from socket */  
        if(recvfrom(sockfd, &SYNACK_packet, sizeof(SYNACK_packet), 0, &from, &from_len) != -1){
            printf("New packet arrived!\n");

            /* If the SYNACK that arrived is valid */
            if(SYNACK_packet.flags == SYNACK && SYNACK_packet.seq == client.seqnum && verify_checksum(&SYNACK_packet) == 1){
                alarm(0);                               /* Stop the timer */
                printf("Valid SYNACK!\n");
                printf("Packet info: Type = %d\tSeq = %d\tWindowsize = %d\n\n", SYNACK_packet.flags, SYNACK_packet.seq, SYNACK_packet.winSize);
                client.seqnum++;                        /* Increase the sequence number */
                break;

            /* If the SYNACK that arrived is invalid */
            } else{
                printf("Invalid SYNACK\n");
                continue;                               /* Continue to listen */
            }

        /* Failed to read from socket */
        } else{
            perror("Can't read from socket");
            exit(EXIT_FAILURE);
        }
    }


    ACK_packet = packetBuild(ACK_packet, ACK);          /* Build the ACK packet */

    /* Send the ACK packet to server */
    if(maybe_sendto(sockfd, &ACK_packet, sizeof(ACK_packet), 0, serverName, socklen) != -1){
        printf("ACK sent: Type = %d\tSeq = %d\n\n", ACK_packet.flags, ACK_packet.seq);

    /* Failed to send ACK */
    } else{
        perror("Send ACK");
        exit(EXIT_FAILURE);
    }

    fcntl(sockfd, F_SETFL, O_NONBLOCK);                 /* Make the socket non-blocking */
    client.state = WAIT_OPEN;                           /* Change client state */
    alarm(TIMEOUT + 2);                                 /* Start a timer */


    /* Continue to listen for new packet (if the ACK was lost)*/
    while(client.state != ESTABLISHED){

        /* Read from socket. If nothing is received, a timeout will occur and client.state = ESTABLISHED */
        if(recvfrom(sockfd, &SYNACK_packet, sizeof(SYNACK_packet), 0, &from, &from_len) != -1){  
            printf("New packet arrived again! Resend ACK\n");
            
            /* New SYNACK, resend the ACK packet again to server */
            if(maybe_sendto(sockfd, &ACK_packet, sizeof(ACK_packet), 0, serverName, socklen) != -1){
                printf("ACK sent: Type = %d\tSeq = %d\n\n", ACK_packet.flags, ACK_packet.seq);
                alarm(TIMEOUT);                         /* Start the timer again */

            /* Failed to send ACK */
            } else{
                perror("Send ACK");
                exit(EXIT_FAILURE);
            }       

        
        } else{

            if(client.state != ESTABLISHED){
                continue;
            
            /* If client.state = ESTABLISHED, the connection was successful */
            } else if(client.state == ESTABLISHED){
                fcntl(client.sockfd, F_SETFL, 0);       /* Make the socket blocking again */
                break;
            }
            
            /* Failed to read from socket */
            perror("Can't read from socket");
            exit(EXIT_FAILURE);
        }
    }

    /* Successfull connection, return 1 */ 
    return 1;                                                        
}


/* DATA sending thread, sends out max windowsize amount of DATA packet each time  */
void *sendthread(void *arg){
    alarm(0);                                       /* Stop timer */
    client.state = SEND_DATA;                       /* Change client state */
    rtp DATA_packet;                                /* Defining DATA packet */
    nextSeq = 0;                                    /* Next sequence number to be sent */
    base = 0;                                       /* Base of sliding window*/              
    int N = client.window_size;                     /* Max UNACK packets allowed */
    DATA_packet = packetBuild(DATA_packet, DATA);   /* Build DATA packet */


    while(1){

        pthread_mutex_lock(&mutex);                 /* Lock mutex */

        /* Send out DATA packet to windowsize limit*/
        if(nextSeq < (base + N)){

            /* Send FIN packet after all DATA packets are sent */
            if(DATA_packet.seq == packetToSend){
                pthread_mutex_unlock(&mutex);       /* Unlock mutex */
                sleep(2);                           /* Wait to receive all ACKs*/
                printf("Sent all DATA\n");
                break;
            }

        /* Send DATA packet to server */
        if(maybe_sendto(client.sockfd, &DATA_packet, sizeof(DATA_packet), 0, client.DestName, client.socklen) != -1){
            printf("DATA sent: Type = %d\tSeq = %d\t\tData = %s\n\n", DATA_packet.flags, DATA_packet.seq, DATA_packet.data);

        /* Failed to send DATA */
        } else{
            perror("Send DATA");
            exit(EXIT_FAILURE);
        }   

        /* If it's the first packet in window to be sent */
        if(base == nextSeq){
            alarm(TIMEOUT);                             /* Start timer */
        }         

        /* Update DATA packet info */
        nextSeq++;
        DATA_packet.seq = nextSeq;                      /* Next sequence number */
        DATA_packet.checksum = 0;                       /* Set checksum to 0 */
        DATA_packet.checksum = checksum(&DATA_packet);  /* Recalculate checksum */

        }

        pthread_mutex_unlock(&mutex);                   /* Unlock mutex */

    }

    /* Successfully sent all DATA packets */
    return NULL;
}


/* ACK receiving thread, updates the base (for sendthread) */
void *recvthread(void *arg){
    rtp ACK_packet;                                     /* Defining ACK packet */
    struct sockaddr from;                               /* Adress from receiver */           
    socklen_t from_len = sizeof(from);                  /* socket length from receiver */ 
    expSeqNum = 0;                                      /* Expected sequence number from server */


    while(1){

        /* Read from socket */  
        if(recvfrom(client.sockfd, &ACK_packet, sizeof(ACK_packet), 0, &from, &from_len) != -1){ 
            printf("New packet arrived!\n");

            /* If the arrived ACK is corrupted */
            if(verify_checksum(&ACK_packet) != 1){
                printf("Corrupt ACK packet! Seq = %d\n\n", ACK_packet.seq);
                continue;                               /* Continue to listen */
            }

            pthread_mutex_lock(&mutex);                 /* Lock mutex */

            /* If the arrived ACK is valid */
            if(ACK_packet.flags == ACK && ACK_packet.seq >= expSeqNum){
                alarm(0);                               /* Stop the timer */
                printf("Valid ACK!\n");
                printf("Packet info: Type = %d\tSeq = %d\n\n", ACK_packet.flags, ACK_packet.seq);
                
                /* If the ACK is the last expected one */
                if(ACK_packet.seq == packetToSend - 1){
                    alarm(0);                           /* Stop the timer */
                    printf("Received the last ACK!\n");
                    break;                              /* Proceed connection teardown */
                }

                base = ACK_packet.seq + 1;  /* Slide the window forward one step */
                expSeqNum = ACK_packet.seq + 1;

                /* If it's the last in-flight ACK */
                if(base == nextSeq){
                    alarm(0);                           /* Stop the timer */

                } else{
                    alarm(TIMEOUT);                     /* Restart timer */

                }

            /* Else the ACK is a duplicate */
            } else{
                printf("Duplicate ACK!\n");                
                printf("Packet info: Seq = %d\n\n", ACK_packet.seq);
                continue;
            }

        /* Failed to read from socket */
        } else{
            perror("Can't read from socket");
            exit(EXIT_FAILURE);
        }

        pthread_mutex_unlock(&mutex);                   /* Unlock mutex */
        usleep(1000);                                   /* Wait 1ms */
    }

    /* Successfully received all ACK */
    return NULL;
}


int sender_teardown(int sockfd, const struct sockaddr *serverName, socklen_t socklen){
    rtp FIN_packet, FINACK_packet, ACK_packet;
    struct sockaddr from;                                   /* Adress from receiver */           
    socklen_t from_len = sizeof(from);                      /* socket length from receiver */ 

    FIN_packet = packetBuild(FIN_packet, FIN);

    if(maybe_sendto(sockfd, &FIN_packet, sizeof(FIN_packet), 0, serverName, socklen) != -1){
        printf("FIN sent: Type = %d\tSeq = %d\n\n", FIN_packet.flags, FIN_packet.seq);
        client.state = WAIT_FINACK;

    /* Failed to send FIN */
    } else{
        perror("Send FIN");
        exit(EXIT_FAILURE);
    }

    client.state = WAIT_FINACK;
    alarm(TIMEOUT);     /* Start a timer */

    while(1){
        /* Read from socket */  
        if(recvfrom(sockfd, &FINACK_packet, sizeof(FINACK_packet), 0, &from, &from_len) != -1){
            printf("New packet arrived!\n");

            if(FINACK_packet.flags == FINACK && FINACK_packet.seq == client.seqnum && verify_checksum(&FINACK_packet) == 1){
                alarm(0);       /* Stop the timer */
                printf("Valid FINACK!\n");
                printf("Packet info: Type = %d\tSeq = %d\n\n", FINACK_packet.flags, FINACK_packet.seq);
                client.seqnum++;
                break;

            } else{
                printf("Invalid FINACK!\n\n");
            }

        /* Failed to read from socket */
        } else{
            perror("Can't read from socket");
            exit(EXIT_FAILURE);
        }
    }

    ACK_packet = packetBuild(ACK_packet, ACK);

    if(maybe_sendto(sockfd, &ACK_packet, sizeof(ACK_packet), 0, serverName, socklen) != -1){
        printf("ACK sent: Type = %d\tSeq = %d\n\n", ACK_packet.flags, ACK_packet.seq);

    /* Failed to send ACK */
    } else{
        perror("Send ACK");
        exit(EXIT_FAILURE);
    }

    client.state = WAIT_CLOSE;
    alarm(TIMEOUT + 2);

    while(client.state != CLOSED){
        
        /* Read from socket */  
        if(recvfrom(sockfd, &FINACK_packet, sizeof(FINACK_packet), 0, &from, &from_len) != -1){  
            printf("New packet arrived again! Resend ACK\n");
            
            if(maybe_sendto(sockfd, &ACK_packet, sizeof(ACK_packet), 0, serverName, socklen) != -1){
                printf("ACK sent: Type = %d\tSeq = %d\n\n", ACK_packet.flags, ACK_packet.seq);
                alarm(TIMEOUT);

            /* Failed to send ACK */
            } else{
                perror("Send ACK");
                exit(EXIT_FAILURE);
            }       

        /* Failed to read from socket */
        } else{
            if (errno == EINTR){
                printf("Timeout occurred\n");
                break;
            } else {
                perror("Can't read from socket");
                exit(EXIT_FAILURE);
            }
        }
    }
    return 1;
}



void timeout_handler(int signum){
    rtp re_packet;


    /* TIMEOUT: SYNACK doesn't arrived in time frame */
    if(client.state == WAIT_SYNACK){
        printf("TIMEOUT: Packet lost or corrupt. Resending SYN\n");
        re_packet = packetBuild(re_packet, SYN);

        if(maybe_sendto(client.sockfd, &re_packet, sizeof(re_packet), 0, client.DestName, client.socklen) != -1){
            printf("SYN sent: Type = %d\tSeq = %d\n\n", re_packet.flags, re_packet.seq);

        /* Failed to send SYN */
        } else{
            perror("Send SYN");
            exit(EXIT_FAILURE);
        }   
        alarm(TIMEOUT);


    /* TIMEOUT: Doesn't receiver a new packet in time fram */
    } else if(client.state == WAIT_OPEN){
        printf("TIMEOUT: Connection successfully established!\n\n\n");
        client.state = ESTABLISHED;
        client.seqnum++;
    

    /* TIMEOUT: Doesn't receive ACK on sent DATA in time frame */
    } else if(client.state == SEND_DATA){
        printf("TIMEOUT: Seq = %d\n", base);
        re_packet = packetBuild(re_packet, DATA);

        for(int i = base; i < nextSeq; i++){
            printf("Resending Seq = %d\n", re_packet.seq);
            if(maybe_sendto(client.sockfd, &re_packet, sizeof(re_packet), 0, client.DestName, client.socklen) != -1){
            printf("DATA sent: Type = %d\tSeq = %d\t\tData = %s\n\n", re_packet.flags, re_packet.seq, re_packet.data);

            /* Failed to send SYN */
            } else{
                perror("Send SYN");
                exit(EXIT_FAILURE);
            } 

            if(base == re_packet.seq){
                alarm(TIMEOUT);
            }
            re_packet.seq++;
            re_packet.checksum = 0;
            re_packet.checksum = checksum(&re_packet);
        }


    /* TIMEOUT: Doesn't receive FINACK in time frame */
    } else if(client.state == WAIT_FINACK){
        printf("TIMEOUT: Packet lost or corrupt. Resending FIN\n");
        re_packet = packetBuild(re_packet, FIN);

        if(maybe_sendto(client.sockfd, &re_packet, sizeof(re_packet), 0, client.DestName, client.socklen) != -1){
            printf("FIN sent: Type = %d\tSeq = %d\n\n", re_packet.flags, re_packet.seq);

        /* Failed to send FIN */
        } else{
            perror("Send FIN");
            exit(EXIT_FAILURE);
        }   
        alarm(TIMEOUT);


    /* TIMEOUT: Dosen't receive new packet in time frame */
    } else if(client.state == WAIT_CLOSE){
        printf("TIMEOUT: Connection successfully closed!\n\n\n");
        client.state = CLOSED;
        client.seqnum++;

        if(close(client.sockfd) < 0){
            perror("Failed to close socket");
        }

        printf("STATE: END OF PROGRAM\n");
        exit(EXIT_SUCCESS);
    }

}


/* Build different packet types */
rtp packetBuild(rtp packet, int type){
    
    /* SYN packet */
    if(type == SYN){
        packet.flags = SYN;
        packet.seq = client.seqnum;
        packet.id = getpid();
        packet.winSize = client.window_size;
        packet.checksum = 0;
        packet.checksum = checksum(&packet);
        return packet;

    /* DATA packet */
    } else if(type == DATA){
        packet.flags = DATA;
        packet.seq = base;
        packet.id = getpid();
        packet.winSize = client.window_size;
        strncpy(packet.data, "Packet from client", sizeof(packet.data));
        packet.checksum = 0;
        packet.checksum = checksum(&packet);
        return packet;

    /* ACK packet */
    } else if(type == ACK){
        packet.flags = ACK;
        packet.seq = client.seqnum;
        packet.id = getpid();
        packet.winSize = client.window_size;
        packet.checksum = 0;
        packet.checksum = checksum(&packet);
        return packet;

    /* FIN packet */
    } else if(type == FIN){
        packet.flags = FIN;
        packet.seq = client.seqnum;
        packet.id = getpid();
        packet.winSize = client.window_size;
        packet.checksum = 0;
        packet.checksum = checksum(&packet);
        return packet;
    }

    printf("Invalid packet build type!\n");
    exit(EXIT_FAILURE);
}


/* Get a random sequence number between 5 and 100 */
int get_randNum(){
    int num;
    srand(time(NULL));
    num = (rand() % (MAX_SEQ - MIN_SEQ + 1)) + MIN_SEQ;
    return num;
}


/* Calculating the checksum of the packet */
unsigned short checksum(const rtp *packet) {
    const unsigned short *ptr = (const unsigned short *)packet; /* Buffer packet */
    int len = (sizeof(rtp) - sizeof(packet->checksum)) / 2;     /* Number of 16-bit words */
    unsigned int sum = 0;                                       /* Calculated checksum */

    while (len--) {
        sum += *ptr++;

        /* Carry occurred, warp around */
        if (sum & 0xFFFF0000) {
            sum &= 0xFFFF;
            sum++;
        }
    }

    return ~sum;                    /* Return calculated checksum */                                           
}


/* Verifying the checksum of the packet */
int verify_checksum(const rtp *packet) {
    unsigned short saved_checksum = packet->checksum;       /* Save the original checksum */
    rtp temp_packet = *packet;                              /* Create a temporary copy of the packet */
    temp_packet.checksum = 0;                               /* Clear the checksum field */

    unsigned short *ptr = (unsigned short *)&temp_packet;   /* Buffer packet */
    int len = (sizeof(rtp) - sizeof(packet->checksum)) / 2; /* Number of 16-bit words */
    unsigned int sum = 0;                                   /* Calculated checksum */

    while (len--) {
        sum += *ptr++;

        /* Carry occurred, warp around */
        if (sum & 0xFFFF0000) {
            sum &= 0xFFFF;
            sum++;
        }
    }

    /* Calculate the one's complement of the sum */
    unsigned short calculated_checksum = ~sum;

    /* Check if the calculated checksum matches the original checksum */
    if (calculated_checksum == saved_checksum) {
        return 1; /* Checksum is correct */

    } else {
        return -1; /* Checksum is incorrect */
    }
}


/* ERROR generator */
ssize_t maybe_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen){
    char *buffer = malloc(len);     /* Create temp buffer */
    memcpy(buffer, buf, len);       /* Copy message to temp buffer */

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
            printf("Simulated corrupt packet \n");
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
    
    /* Packet lost */
    } else{ 
        printf("Simulated lost packet \n");
        return(len);
    }
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
    struct sockaddr_in serverName;
    char hostName[hostNameLength];      /* Name of the host/receiver */
    pthread_t t1, t2;

    socklen_t socklen = sizeof(struct sockaddr);    /* Length of the socket structure sockaddr */

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

    /* Initialize client info */
    client.state = CLOSED;
    client.seqnum = 0;
    client.window_size = windowSize;
    client.sockfd = sockfd;
    client.DestName = (struct sockaddr*)&serverName;
    client.socklen = socklen;

    /* Initilitze timeout handler */
    signal(SIGALRM, timeout_handler);

    /* Start a connection to the server */
    printf("STATE: CLOSED\n\n");
    if(sender_connect(sockfd, (struct sockaddr*)&serverName, socklen) != 1){
        perror("sender connection");
        exit(EXIT_FAILURE);
    }


    /* Start send & receiv thread */
    printf("=================================================================\n\n\n");
    printf("STATE: SENDING DATA\n\n");
    if(pthread_create(&t1, NULL, sendthread, NULL) != 0){
        perror("Failed to create thread");
        exit(EXIT_FAILURE);
    } 

    if(pthread_create(&t2, NULL, recvthread, NULL) != 0){
        perror("Failed to create thread");
        exit(EXIT_FAILURE);
    }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    

    /* Update client info */
    client.seqnum = 0;

    /* Close the socket */
    printf("\n\n=================================================================\n\n");
    printf("\n\nSTATE: START TEARDOWN\n\n");    
    if (sender_teardown(sockfd, (struct sockaddr*)&serverName, socklen) != 1){
        perror("teardown");
        exit(EXIT_FAILURE);
    }

    if(close(sockfd) < 0){
        perror("Failed to close socket");
    }

    printf("\nSTATE: END OF PROGRAM\n");
    return 0;
}

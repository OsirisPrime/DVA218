/*
===============================================================================
File Name       : server.c
Authurs         : Kim Svedberg & Zebastian Thorsén 
Version         : 1.0
Description     : A reliable GBN transfer protocol built on top of UDP.
                  Contains code for the receiver/server side.
===============================================================================
*/

#include "gbn.h"

/* Varibles for recv thread */
int expSeqNum;
state_t server;


/* Connection function. Receives SYN, send SYNACK and rcvs ACK */
int receiver_connect(int sockfd, const struct sockaddr *client, socklen_t *socklen){
    rtp SYN_packet, SYNACK_packet, ACK_packet;              /* Defining the packets */
    struct sockaddr from;                                   /* Adress from receiver */           
    socklen_t from_len = sizeof(from);                      /* socket length from receiver */
    server.state = LISTENING;                               /* Change state */

    
    while(1){

        /* Read from socket */
        if(recvfrom(sockfd, &SYN_packet, sizeof(SYN_packet), 0, client, socklen) != -1){
            printf("New packet arrived!\n");

            /* If the SYN that arrived is valid */
            if(SYN_packet.flags == SYN && SYN_packet.seq == server.seqnum && verify_checksum(&SYN_packet) == 1){
                printf("Valid SYN!\n");
                printf("Packet info: Type = %d\tSeq = %d\tWindowsize = %d\n\n", SYN_packet.flags, SYN_packet.seq, SYN_packet.winSize);
                
                /* Update server info */
                server.window_size = SYN_packet.winSize;
                break;

            } else{
                printf("Invalid SYN. Continue to listen\n");
            }

        } else{
            perror("Can't read from socket");
            exit(EXIT_FAILURE);
        }
    }

    SYNACK_packet = packetBuild(SYNACK_packet, SYNACK);     /* Build SYNACK packet */

    /* Send SYNACK to client */
    if(maybe_sendto(sockfd, &SYNACK_packet, sizeof(SYNACK_packet), 0, client, *socklen) != -1){
        printf("SYNACK sent: Type = %d\tSeq = %d\tWindowsize = %d\n\n", SYNACK_packet.flags, SYNACK_packet.seq, SYNACK_packet.winSize);

    /* Failed to send SYNACK */
    } else{
        perror("Send SYNACK");
        exit(EXIT_FAILURE);
    }
    
    server.state = WAIT_OPEN_ACK;                           /* Change state */
    alarm(TIMEOUT);                                         /* Start a timer */

    while(1){

        /* Read from socket */  
        if(recvfrom(sockfd, &ACK_packet, sizeof(ACK_packet), 0, &from, &from_len) != -1){
            printf("New packet arrived!\n");

            /* If the arrived ACK is valid */
            if(ACK_packet.flags == ACK && ACK_packet.seq == server.seqnum + 1 && verify_checksum(&ACK_packet) == 1){
                alarm(0);               /* Stop the timer */
                printf("Valid ACK!\n");
                printf("Packet info: Type = %d\tSeq = %d\n\n", ACK_packet.flags, ACK_packet.seq);
                break;

            /* Else rcvd SYN again */
            } else if(ACK_packet.flags == SYN){
                alarm(0);               /* Stop the timer */

                /* Send SYNACK to client again */
                if(maybe_sendto(sockfd, &SYNACK_packet, sizeof(SYNACK_packet), 0, client, *socklen) != -1){
                    printf("SYN arrived again\n");
                    printf("SYNACK sent: Type = %d\tSeq = %d\tWindowsize = %d\n\n", SYNACK_packet.flags, SYNACK_packet.seq, SYNACK_packet.winSize);
                    alarm(TIMEOUT);     /* Start a timer again */

                /* Failed to send SYNACK */
                } else{
                    perror("Send SYNACK");
                    exit(EXIT_FAILURE);
                }
            }

        /* Failed to read from socket */
        } else{
            perror("Can't read from socket");
            exit(EXIT_FAILURE);

        }
    }
    
    /* Update server info */
    server.seqnum++;
    server.state = ESTABLISHED;
    printf("Connection successfully established!\n\n\n");
    return 1;
}


/* DATA receiving & ACK sending thread */
void *recvthread(void *arg){
    rtp DATA_packet, ACK_packet;                        /* Defining packets */
    struct sockaddr from;                               /* Adress from receiver */           
    socklen_t from_len = sizeof(from);                  /* socket length from receiver */
    expSeqNum = 0;                                      /* Expected sequence number */


    while(1){
        alarm(20);              /* Start a timer */

        /* Read from socket */
        if(recvfrom(server.sockfd, &DATA_packet, sizeof(DATA_packet), 0, &from, &from_len) != -1){
            printf("New packet arrived!\n");
            alarm(0);           /* Stop the timer */

            /* If the arrived DATA is valid */
            if(DATA_packet.flags == DATA && DATA_packet.seq == expSeqNum && verify_checksum(&DATA_packet) == 1){
                printf("Valid DATA!\n");
                printf("Packet info: Type = %d\tSeq = %d\t\tData = %s\n\n", DATA_packet.flags, DATA_packet.seq, DATA_packet.data);

                /* Update ACK packet info */
                ACK_packet.flags = ACK;
                ACK_packet.seq = expSeqNum;
                ACK_packet.id = DATA_packet.id;
                ACK_packet.winSize = server.window_size;
                ACK_packet.checksum = 0;
                ACK_packet.checksum = checksum(&ACK_packet);
                expSeqNum++;                    /* Increase the expected sequence number */         

                /* Send ACK to client */
                if(maybe_sendto(server.sockfd, &ACK_packet, sizeof(ACK_packet), 0, server.DestName, server.socklen) != -1){
                    printf("ACK sent: Type = %d\tSeq = %d\n\n", ACK_packet.flags, ACK_packet.seq);
                
                /* Failed to send ACK */
                } else{
                    perror("Send ACK");
                    exit(EXIT_FAILURE);
                }

            /* If the arrived packet is a valid FIN*/
            } else if(DATA_packet.flags == FIN && DATA_packet.seq == 0 && verify_checksum(&DATA_packet) == 1){
                printf("Received FIN packet! - Start connection teardown\n");
                return NULL;        /* Break and start Teardown function*/

            /* Else the DATA is invalid (corrupt or out of order */
            }else{
                printf("Invalid DATA\n"); 

                /* The DATA is corrupted */
                if(verify_checksum(&DATA_packet) != 1){
                    printf("Corrupt DATA packet! Seq = %d\t", DATA_packet.seq);
                    printf("Checksum = %d\t", DATA_packet.checksum);
                    DATA_packet.checksum = 0;
                    printf("Calc checksum = %d\n\n", checksum(&DATA_packet));

                /* The DATA is out of order */
                } else{
                    printf("Out of order DATA packet!\tExpected seq = %d\tReceived seq = %d\n\n", ACK_packet.seq + 1, DATA_packet.seq);                      
                }

                /* Send duplicate ACK to client */
                if(maybe_sendto(server.sockfd, &ACK_packet, sizeof(ACK_packet), 0, server.DestName, server.socklen) != -1){
                    printf("Duplicate ACK sent: Type = %d\tSeq = %d\n\n", ACK_packet.flags, ACK_packet.seq);
                
                /* Failed to send ACK */
                } else{
                    perror("Send ACK");
                    exit(EXIT_FAILURE);
                }  
                continue;
            }

        /* Failed to read from socket */
        } else{
            perror("Can't read from socket");
            exit(EXIT_FAILURE);
        }        

    }
    return NULL;
}


/* Teardown function. Received FIN, sends FINACK and rcvs ACK */
int receiver_teardown(int sockfd, const struct sockaddr *client, socklen_t socklen){
    rtp FIN_packet, FINACK_packet, ACK_packet;              /* Defining the packets */
    struct sockaddr from;                                   /* Adress from receiver */           
    socklen_t from_len = sizeof(from);                      /* socket length from receiver */

    FINACK_packet = packetBuild(FINACK_packet, FINACK);     /* Build FINACK packet */

    /* Send FINACK packet to client */
    if(maybe_sendto(sockfd, &FINACK_packet, sizeof(FINACK_packet), 0, client, socklen) != -1){
        printf("FINACK sent: Type = %d\tSeq = %d\n\n", FINACK_packet.flags, FINACK_packet.seq);

    /* Failed to send FINACK */
    } else{
        perror("Send FINACK");
        exit(EXIT_FAILURE);
    }
    
    server.state = WAIT_CLOSE_ACK;          /* Change state */
    alarm(TIMEOUT);                         /* Start a timer */

    while(1){

        /* Read from socket */  
        if(recvfrom(sockfd, &ACK_packet, sizeof(ACK_packet), 0, &from, &from_len) != -1){
            printf("New packet arrived!\n");

            /* If the arrived ACK is valid */
            if(ACK_packet.flags == ACK && ACK_packet.seq == server.seqnum + 1 && verify_checksum(&ACK_packet) == 1){
                alarm(0);                   /* Stop the timer */
                printf("Valid ACK!\n");
                printf("Packet info: Type = %d\tSeq = %d\n\n", ACK_packet.flags, ACK_packet.seq);                
                break;

            /* Else the ACK is invalid */
            } else{
                printf("Invalid ACK\n\n");
                printf("Packet info: Type = %d\tSeq = %d\tExpSeq = %d\n\n", ACK_packet.flags, ACK_packet.seq, server.seqnum + 1);
                continue;
            }

        /* Failed to read from socket */
        } else{
            perror("Can't read from socket");
            exit(EXIT_FAILURE);
        }
    }

    /* Update server info */
    server.seqnum++;
    server.state = CLOSED;
    printf("ACK received: Connection successfully closed!\n\n\n");  
    return 1; 
}


/* Timeout handler */
void timeout_handler(int signum){
    rtp re_packet;      /* Define packet to resend */


    /* TIMEOU: Doesn't receive new SYN or ACK in time frame */
    if(server.state == WAIT_OPEN_ACK){
        re_packet = packetBuild(re_packet, SYNACK);

        printf("TIMEOUT: No ACK packet received. Assuming SYNACK was lost, resend SYNACK\n");
        /* Send SYNACK to client again */
        if(maybe_sendto(server.sockfd, &re_packet, sizeof(re_packet), 0, server.DestName, server.socklen) != -1){
            printf("SYNACK sent: Type = %d\tSeq = %d\tWindowsize = %d\n\n", re_packet.flags, re_packet.seq, re_packet.winSize);
            alarm(TIMEOUT);     /* Start a timer again */

        /* Failed to send SYNACK */
        } else{
            perror("Send SYNACK");
            exit(EXIT_FAILURE);
        }


    /* TIMEOUT: Doesn't receive any DATA in time frame */
    } else if(server.state == ESTABLISHED){
        printf("TIMEOUT: No new data from client - Assuming client has disconnected - Closing socket\n");
        
        /* Close the socket */
        if(close(server.sockfd) < 0){
            perror("Failed to close the socket");
        }
        exit(EXIT_SUCCESS);


    /* TIMEOUT: Doesn't receive ACK in time fram */
    } else if(server.state == WAIT_CLOSE_ACK){
        printf("TIMEOUT: Packet lost or corrupt. Resending FINACK\n");
        re_packet = packetBuild(re_packet, FINACK);

        if(maybe_sendto(server.sockfd, &re_packet, sizeof(re_packet), 0, server.DestName, server.socklen) != -1){
            printf("FINACK sent: Type = %d\tSeq = %d\n\n", re_packet.flags, re_packet.seq);

        /* Failed to send FINACK */
        } else{
            perror("Send FINACK");
            exit(EXIT_FAILURE);
        }
        alarm(TIMEOUT);
    }

}


/* Build different packet types */
rtp packetBuild(rtp packet, int type){
    
    /* SYNACK packet */
    if(type == SYNACK){
        packet.flags = SYNACK;
        packet.seq = server.seqnum;
        packet.id = getpid();
        packet.winSize = server.window_size;
        packet.checksum = 0;
        packet.checksum = checksum(&packet);
        return packet;

    /* ACK packet */
    } else if(type == ACK){
        packet.flags = ACK;
        packet.seq = server.seqnum;
        packet.id = getpid();
        packet.winSize = server.window_size;
        packet.checksum = 0;
        packet.checksum = checksum(&packet);
        return packet;

    /* FINACK packet */
    } else if(type == FINACK){
        packet.flags = FINACK;
        packet.seq = server.seqnum;
        packet.id = getpid();
        packet.winSize = server.window_size;
        packet.checksum = 0;
        packet.checksum = checksum(&packet);
        return packet;

    }
    printf("Invalid packet build type!\n");
    exit(EXIT_FAILURE);
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
    char *buffer = malloc(len);             /* Create temp buffer */
    memcpy(buffer, buf, len);               /* Copy message to temp buffer */
    srand(time(NULL));                      /* Initialize random number generator */

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
        return result;                      /* Return number of bytes sent */     
    
    /* Packet lost */
    } else{ 
        printf("Simulated lost packet \n");
        return(len);                        /* Return fake number of bytes sent */
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
    int sockfd;                         /* Socket file descriptor of the receiver */   
    pthread_t t1;                       /* Define pthread */
    socklen_t socklen;                  /* Length of the socket structure sockaddr */
    struct sockaddr_in clientName;      /* Client adress */
    socklen = sizeof(struct sockaddr_in);


    /* Create a socket and set it up to accept connections */
    sockfd = makeSocket(PORT);

    /* Initialize server info */
    server.state = CLOSED;
    server.seqnum = 0;
    server.window_size = 0;
    server.sockfd = sockfd;
    server.DestName = (struct sockaddr*)&clientName;
    server.socklen = socklen;

    /* Initilize timeout handler */
    signal(SIGALRM, timeout_handler);       

    /* Listen to socket */
    printf("STATE: LISTENING\n\n");
    if(receiver_connect(sockfd, (struct sockaddr*)&clientName, &socklen) != 1){
        perror("receiver_connection");
        exit(EXIT_FAILURE);  
    }


    /* Create thread that receives DATA & send ACK */
    printf("=================================================================\n\n\n");
    printf("STATE: WAITING FOR DATA\n\n");
    if(pthread_create(&t1, NULL, recvthread, NULL) != 0){
        perror("Failed to create thread");
        exit(EXIT_FAILURE);
    }
    pthread_join(t1, NULL);


    /* Update server info */
    server.seqnum = 0;

    /* Teardown the socket */ 
    printf("\n\n=================================================================\n\n");
    printf("\n\nSTATE: START TEARDOWN\n\n");
    if(receiver_teardown(sockfd, (struct sockaddr*)&clientName, socklen) != 1){
        perror("receiver_teardown");
        exit(EXIT_FAILURE);
    }

    /* Close the socket */
    if(close(sockfd) < 0){
        perror("Failed to close socket");
    }

    printf("\nSTATE: END OF PROGRAM\n");
    return 0;
}

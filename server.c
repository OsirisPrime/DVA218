/*
==========================================================================
File            : server.c
Authurs         : Kim Svedberg & Zebastian Thorsén 
Version         : 1.0
Description     : A reliable GBN transfer  protocol built on top of UDP
==========================================================================
*/

#include "gbn.h"

state_t server;


int receiver_connect(int sockfd, const struct sockaddr *client, socklen_t *socklen){
    rtp SYN_packet, SYNACK_packet, ACK_packet;
    struct sockaddr from;                                   /* Adress from receiver */           
    socklen_t from_len = sizeof(from);                      /* socket length from receiver */

    server.state = LISTENING;
    printf("STATE: LISTENING\n\n");
    
    while(1){
        if(recvfrom(sockfd, &SYN_packet, sizeof(SYN_packet), 0, client, socklen) != -1){
            printf("New packet arrived!\n");

            if(SYN_packet.flags == SYN && verify_checksum(&SYN_packet) == 1){
                printf("Valid SYN!\n");
                printf("Packet info: Type = %d\tSeq = %d\tWindowsize = %d\n\n", SYN_packet.flags, SYN_packet.seq, SYN_packet.winSize);
                
                server.seqnum = SYN_packet.seq;
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

    SYNACK_packet = packetBuild(SYNACK_packet, SYNACK);

    if(maybe_sendto(sockfd, &SYNACK_packet, sizeof(SYNACK_packet), 0, client, *socklen) != -1){
        printf("SYNACK sent: Type = %d\tSeq = %d\tWindowsize = %d\n\n", SYNACK_packet.flags, SYNACK_packet.seq, SYNACK_packet.winSize);

    /* Failed to send SYNACK */
    } else{
        perror("Send SYNACK");
        exit(EXIT_FAILURE);
    }
    
    server.state = WAIT_OPEN_ACK;
    alarm(TIMEOUT + 2);     /* Start a timer */

    while(1){
        /* Read from socket */  
        if(recvfrom(sockfd, &ACK_packet, sizeof(ACK_packet), 0, &from, &from_len) != -1){
            printf("New packet arrived!\n");

            if(ACK_packet.flags == ACK && ACK_packet.seq == server.seqnum + 1 && verify_checksum(&ACK_packet) == 1){
                alarm(0);       /* Stop the timer */
                printf("Valid ACK!\n");
                printf("Packet info: Type = %d\tSeq = %d\n\n", ACK_packet.flags, ACK_packet.seq);

                break;

            } else{
                printf("Invalid ACK\n");
            }

        /* Failed to read from socket */
        } else{
            perror("Can't read from socket");
            exit(EXIT_FAILURE);
        }
    }
    
    server.seqnum++;
    server.state = ESTABLISHED;
    printf("ACK received. Connection successfully established!\n\n\n");
    return 1;
}


void *sendthread(void *arg){

}


void *recvthread(void *arg){

}



int receiver_teardown(int sockfd, const struct sockaddr *client, socklen_t socklen){
    rtp FIN_packet, FINACK_packet, ACK_packet;
    struct sockaddr from;                                   /* Adress from receiver */           
    socklen_t from_len = sizeof(from);                      /* socket length from receiver */

    printf("STATE: Received FIN\n\n");

    FINACK_packet = packetBuild(FINACK_packet, FINACK);

    if(maybe_sendto(sockfd, &FINACK_packet, sizeof(FINACK_packet), 0, client, socklen) != -1){
        printf("FINACK sent: Type = %d\tSeq = %d\n\n", FIN_packet.flags, FIN_packet.seq);

    /* Failed to send FINACK */
    } else{
        perror("Send FINACK");
        exit(EXIT_FAILURE);
    }
    
    server.state = WAIT_CLOSE_ACK;
    alarm(TIMEOUT);     /* Start a timer */

    while(1){
        /* Read from socket */  
        if(recvfrom(sockfd, &ACK_packet, sizeof(ACK_packet), 0, &from, &from_len) != -1){
            printf("New packet arrived!\n");

            if(ACK_packet.flags == ACK && ACK_packet.seq == server.seqnum + 1 && verify_checksum(&ACK_packet) == 1){
                alarm(0);       /* Stop the timer */
                printf("Valid ACK!\n");
                printf("Packet info: Type = %d\tSeq = %d\n\n", ACK_packet.flags, ACK_packet.seq);

                break;

            } else{
                printf("Invalid ACK\n");
            }

        /* Failed to read from socket */
        } else{
            perror("Can't read from socket");
            exit(EXIT_FAILURE);
        }
    }
    
    server.seqnum++;
    server.state = CLOSED;
    printf("ACK received. Connection successfully closed!\n\n\n");
    return 1; 
}


void timeout_handler(int signum){
    rtp re_packet;

    if(server.state == WAIT_OPEN_ACK){
        printf("TIMEOUT: Packet lost or corrupt. Resending SYNACK\n");
        re_packet = packetBuild(re_packet, SYNACK);

        if(maybe_sendto(server.sockfd, &re_packet, sizeof(re_packet), 0, server.DestName, server.socklen) != -1){
            printf("SYNACK sent: Type = %d\tSeq = %d\n\n", re_packet.flags, re_packet.seq);

        /* Failed to send SYNACK */
        } else{
            perror("Send SYNACK");
            exit(EXIT_FAILURE);
        }
        alarm(TIMEOUT + 2);


    } else if(server.state == WAIT_CLOSE_ACK){
        printf("TIMEOUT: Packet lost or corrupt. Resending FINACK\n");
        re_packet = packetBuild(re_packet, SYNACK);

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


rtp packetBuild(rtp packet, int type){
    
    if(type == SYNACK){
        packet.flags = SYNACK;
        packet.seq = server.seqnum;
        packet.id = getpid();
        packet.winSize = server.window_size;
        packet.checksum = 0;
        packet.checksum = checksum(&packet);
        return packet;

    } else if(type == ACK){
        packet.flags = ACK;
        packet.seq = server.seqnum;
        packet.id = getpid();
        packet.winSize = server.window_size;
        packet.checksum = 0;
        packet.checksum = checksum(&packet);
        return packet;

    } else if(type == FINACK){
        packet.flags = FINACK;
        packet.seq = server.seqnum;
        packet.id = getpid;
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
    const unsigned short *ptr = (const unsigned short *)packet;
    int len = (sizeof(rtp) - sizeof(packet->checksum)) / 2; // Number of 16-bit words
    unsigned int sum = 0;

    while (len--) {
        sum += *ptr++;
        if (sum & 0xFFFF0000) {
            // Carry occurred, wrap around
            sum &= 0xFFFF;
            sum++;
        }
    }

    return ~sum;
}


/* Verifying the checksum of the packet */
int verify_checksum(const rtp *packet) {
    unsigned short saved_checksum = packet->checksum; // Save the original checksum
    rtp temp_packet = *packet; // Create a temporary copy of the packet
    temp_packet.checksum = 0; // Clear the checksum field

    unsigned short *ptr = (unsigned short *)&temp_packet;
    int len = (sizeof(rtp) - sizeof(packet->checksum)) / 2; // Number of 16-bit words
    unsigned int sum = 0;

    while (len--) {
        sum += *ptr++;
        if (sum & 0xFFFF0000) {
            // Carry occurred, wrap around
            sum &= 0xFFFF;
            sum++;
        }
    }

    // Calculate the one's complement of the sum
    unsigned short calculated_checksum = ~sum;

    // Check if the calculated checksum matches the original checksum
    if (calculated_checksum == saved_checksum) {
        return 1; // Checksum is correct
        
    } else {
        return -1; // Checksum is incorrect
    }
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
    pthread_t t1, t2;           
    socklen_t socklen;              /* Length of the socket structure sockaddr */
    struct sockaddr_in clientName;

    /* Create a socket and set it up to accept connections */
    sockfd = makeSocket(PORT);

    socklen = sizeof(struct sockaddr_in);

    /* Initialize server info */
    server.state = CLOSED;
    server.seqnum = 0;
    server.window_size = 0;
    server.sockfd = sockfd;
    server.DestName = (struct sockaddr*)&clientName;
    server.socklen = socklen;

    signal(SIGALRM, timeout_handler);

    /* Listen to socket */
    if(receiver_connect(sockfd, (struct sockaddr*)&clientName, &socklen) != 1){
        perror("receiver_connection");
        exit(EXIT_FAILURE);  
    }

/*                             

    //Create thread that sends ACK 
    if(pthread_create(&t1, NULL, sendthread, NULL) != 0){
        perror("Failed to create thread");
        exit(EXIT_FAILURE);
    } 

    //Create thread that receives DATA 
    if(pthread_create(&t2, NULL, recvthread, NULL) != 0){
        perror("Failed to create thread");
        exit(EXIT_FAILURE);
    }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL)




    // Close the socket 
    if(receiver_teardown(sockfd, (struct sockaddr*)&clientName, socklen) == -1){
        perror("receiver_teardown");
        exit(EXIT_FAILURE);
    }


*/

    return 0;
}

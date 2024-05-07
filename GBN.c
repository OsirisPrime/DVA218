#include "GBN.h"

#define hostNameLength 50
#define windowSize 1
#define MAXMSG 1024




int threeWayHandshake(int sock, struct sockaddr_in serverName){
    struct timeval timeout;
    char buffer[MAXMSG];
    srand(time(NULL));
    int nOfBytes;
    rtp packet;    
    fd_set activeFdSet;

    /*Timeout values*/
    timeout.tv_sec = 0;
    timeout.tv_usec = 100;
    int seqNum = (rand() % 10000) + 10;


    
    packet.flags = SYN;
    packet.seq = seqNum;
    packet.windowsize = windowSize;

    nOfBytes = sendto(sock, &packet, sizeof(rtp), 0, (struct sockaddr *)&serverName, sizeof(serverName));
    if(nOfBytes < 0){
        perror("theeWayHandshake - Could not send SYN packet\n");
        exit(EXIT_FAILURE);
    }

    FD_Zero(&activeFdSet);
    FD_SET(sock, &activeFdSet);

    /*SYN_ACK Wait state*/
    while(1){
        int result = select(sock + 1, &activeFdSet, NULL, NULL, &timeout);

        if(result == -1){
            perror("Select failed\n");
            exit(EXIT_FAILURE);
        }
        else if(result == 0){
            printf("threeWayHandshake - SYN packet loss");
            nOfBytes = sendto(sock, &packet, sizeof(rtp), 0, (struct sockaddr *)&serverName, sizeof(serverName));
            if(nOfBytes < 0){
                perror("theeWayHandshake - Could not send SYN packet\n");
                exit(EXIT_FAILURE);
            }            
        }
        else{
            break;
        }
    }

    /*Receive packet from server*/
    nOfBytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&serverName, sizeof(serverName));

    memcpy(&packet, buffer, sizeof(rtp));

    /*Check if packet is ACK*/
    if(packet.flags != SYN || packet.flags != ACK){

    }


}


int teardown(int sock, struct sockaddr_in serverName){

}


int validate_checksum(rtp *packet){
    uint16_t received_checksum = packet->checksum;
    packet->checksum = 0;
    uint16_t calculated_checksum = checksum((uint16_t*)packet, sizeof(*packet) / sizeof(uint16_t));

    if(received_checksum == calculated_checksum){
        return 1;
    }
    else{
        printf("Incorrect checksum detected!. Recieved: %d, Calculated: %d\n", received_checksum, calculated_checksum);
        return 0;
    }
}

int checksum(uint16_t *buffer, int len){
    uint32_t sum = 0;   /*Store 32 bit in sum*/

    for(int i = 0; i < len; i++){
        sum += *buffer;
    }

    return sum;
}

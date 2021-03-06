#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "../include/reliable_udp.h"
#include <time.h>

int mss=1500;
#define CLIENT_PORT "8000"
//	Write the data into the file from sender.

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void send_to_user(char* buf){
	printf("Got data %s\n",buf);
}


int verify_checksum(){
	return 0;
}

int32_t get_sequence_number(char *buf){
	printf("Getting ACK value\n");
	int32_t sequence=atoi(buf);
	return sequence;
	
}

int main(int argc, char *argv[]){
	if ( argc < 2){
		printf("Insufficient arguments .. Need MSS  and ( later file name ) \n");
		exit(1);
	}
// 	Create a socket
	srand(time(NULL));
	int sockfd;
	struct addrinfo hints,*p,*servinfo;
	struct sockaddr_storage their_addr;
	char buf[MAXBUFLEN];
    	socklen_t addr_len;
    	char s[INET6_ADDRSTRLEN];
	int rv,numbytes=0;
	int32_t expected_sequence_number=0;
	float error_probablity = atof(argv[3]);
	char *file_name= argv[2];
	memset(&hints,0,sizeof(hints));
	hints.ai_family=AF_INET;
	hints.ai_socktype=SOCK_DGRAM;
	mss=atoi(argv[1]);
	memset(&their_addr,0,sizeof(their_addr));
	if ((rv = getaddrinfo(NULL, P2MP_SERVER_PORT, &hints, &servinfo)) != 0) {
        	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        	return 1;
    	}
	for ( p= servinfo ; p!= NULL ; p = p->ai_next){
		if((sockfd=socket(p->ai_family,p->ai_socktype, p->ai_protocol))==-1){
			close(sockfd);
			perror("socket :");
			continue;
		}
		if ( bind(sockfd,p->ai_addr,p->ai_addrlen) == -1){
			perror("Bind :");
			close(sockfd);
			continue;
		}
		break;
	}	
	if ( p == NULL ){
		fprintf(stderr, "listener: failed to bind socket\n");
        	return 2;
	}
	freeaddrinfo(servinfo);
	char destination_ip[INET_ADDRLEN]={0};
	float current_probability = 0.0;   
//	Check for Data on the socket
	while(1){
//		printf("listener: waiting to recvfrom...\n");
		memset(buf,0,sizeof(buf));
		memset(&their_addr,0,sizeof(their_addr));
		memset(destination_ip,0,sizeof(destination_ip));
		addr_len = sizeof their_addr;
		
		if ((numbytes = recvfrom(sockfd, buf, mss , 0,
			(struct sockaddr *)&their_addr, &addr_len)) == -1) {
			perror("recvfrom");
			exit(1);
		}
		current_probability = (float)rand()/ (float)RAND_MAX;
		printf("Random float is %f\n",current_probability);
		if ( current_probability <= error_probablity ){
			printf("************** Discard packet with error probability *************** %f <= %f \n",current_probability,error_probablity);
			continue;
		}
		 
		strncpy(destination_ip,inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),s, sizeof s),strlen(inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),s, sizeof s)));
//		printf("listener: packet is %d bytes long from destination %s\n", numbytes,destination_ip);
		buf[numbytes] = '\0';
//		printf("listener: packet contains \"%s\"\n", buf);

	//	If received data, verify checksum 
		if(verify_checksum() == 1){
			continue;
		}
	//	If checksum wrong discard the packet
		int32_t sequence_number,ack_sequence_number;
		sequence_number=get_sequence_number(buf);
		if ( sequence_number != expected_sequence_number ){
			printf("Got wrong sequence number\n");
			ack_sequence_number=expected_sequence_number;
		}
		else{	
			printf("Got expected sequence number\n");
			ack_sequence_number=sequence_number+1;
			expected_sequence_number += 1;
		}	
		printf("--------------------   RECIEVED : %d  ------------  \n\n",sequence_number);
		send_to_user(buf);
		sleep(3);
//		printf("Got sequence num : %d and i expected value %d\n", sequence_number,expected_sequence_number);
	//	Check for sequence number..
		memset(buf,0,sizeof(buf));
		snprintf(buf,sizeof(buf),"%d",ack_sequence_number);
	//	If sequence number is wrong, send ACK for required packet
		if ((numbytes = sendto(sockfd, buf, strlen(buf)+1, 0,(struct sockaddr *)&their_addr, addr_len)) == -1) {
			perror("talker: sendto");
		}	
	
		printf("--------------------   ASKING : %d  ------------  and %d \n\n",ack_sequence_number,numbytes);
		sleep(3);
	//	Send the data to the application 
//		printf("Got correct sequence number\n");

//	TNRC : Change 1 to size of data 
	}
    	close(sockfd);
	return 0;
}

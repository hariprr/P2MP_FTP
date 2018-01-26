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
#include <signal.h>

int mss=1500;
int payload_len;
FILE *fp;
int sockfd;
uint16_t ones_complement=0b1111111111111111;
#define OTHER_HEADER 8

#define CLIENT_PORT "8000"
//	Write the data into the file from sender.
void sig_handler(int signo){
        if(signo == SIGINT ){
                printf("Client is shutting down.\n");
		fclose(fp);
		close(sockfd);
                exit(0);
        }
}


int16_t add_16_bit(int16_t num1, int16_t num2){
//	printf("Adding %02x and %02x\n",num1 & 0xFFFF ,num2 & 0xFFFF);
	if ( num1 == 0 )
		return num2;
	if ( num2 == 0 )
		return num1;
	int16_t carry = num1 & num2;
	int16_t sum = num1 ^ num2;
	sum = add_16_bit(sum,carry << 1);
	return sum;
}

uint16_t generate_checksum(char *buf, int payload_len){
	uint16_t *checksum_ptr,checksum_value=0,*end_ptr,current_value;
	uint32_t packet_sequence_number;
	uint16_t packet_checksum,packet_msg_type;
	memcpy((void *)&packet_sequence_number,&buf[0],sizeof(packet_sequence_number));
	memcpy((void *)&packet_msg_type,&buf[6],sizeof(packet_msg_type));
	
	// Take sequence number and msg_type into consideration for checksum	
	checksum_value=packet_msg_type;
	checksum_ptr = (uint16_t *)&packet_sequence_number;
	checksum_value=add_16_bit(checksum_value,*checksum_ptr);	
	checksum_ptr += 1;
	checksum_value=add_16_bit(checksum_value,*checksum_ptr);	
	char *padded_end_ptr=NULL;
	checksum_ptr=(int16_t *)&buf[8];
	if( payload_len % 2 == 0 ){
		end_ptr=(uint16_t *)&buf[payload_len];
	}
	else{
//		printf("Adding the padding bit in the mix\n");
		padded_end_ptr=(char* )&buf[payload_len];
		padded_end_ptr++;
		end_ptr=(uint16_t *)padded_end_ptr;	
	}
	while ( checksum_ptr != end_ptr){
//		printf("Adding %02x and %02x\n",checksum_value,*checksum_ptr);
		checksum_value = add_16_bit(checksum_value,*checksum_ptr);
//		printf("Checksum is %d\n",checksum_value);
		checksum_ptr +=1;
	}
	printf("Checksum is %d\n",checksum_value);
	packet_checksum=ntohs(checksum_value ^ ones_complement);
	memcpy(&buf[4],(void *)&packet_checksum,sizeof(packet_checksum));
	return checksum_value;
}

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
int verify_checksum(int32_t recv_packet_checksum,char *buf,int numbytes){
	printf("My buf length is %d\n",numbytes);
	uint16_t new_checksum=generate_checksum(buf,numbytes);
	if( recv_packet_checksum ^ new_checksum == ones_complement){
		return 0;
	}
	return 1;
}

int main(int argc, char *argv[]){
	if ( argc < 2){
		printf("Insufficient arguments .. Need MSS  and ( later file name ) \n");
		exit(1);
	}
// 	Create a socket
	srand(time(NULL));
	struct addrinfo hints,*p,*servinfo;
	struct sockaddr_storage their_addr;
	char buf[MAXBUFLEN];
    	socklen_t addr_len;
    	char s[INET6_ADDRSTRLEN];
	int rv,numbytes=0;
	int32_t expected_sequence_number=0;
	float error_probablity = atof(argv[3]);
	char *file_name= argv[2];
        uint32_t packet_sequence_number=0,recv_packet_sequence_number=0;
        uint16_t packet_checksum=0,recv_packet_checksum=0;
        uint16_t packet_msg_type=0,recv_packet_msg_type=0;

	memset(&hints,0,sizeof(hints));
	hints.ai_family=AF_INET;
	hints.ai_socktype=SOCK_DGRAM;
	mss=atoi(argv[1]);
	memset(&their_addr,0,sizeof(their_addr));
	if ((rv = getaddrinfo(argv[4], P2MP_SERVER_PORT, &hints, &servinfo)) != 0) {
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
	fp = fopen(file_name,"w");
	if ( !fp ){
		printf("Cannot open file for writing\n");
		exit(1);
	}
	if(signal(SIGINT,sig_handler) == SIG_ERR){
                printf("Cant register SIGTERM\n");
        }

//	Check for Data on the socket
	while(1){
//		printf("listener: waiting to recvfrom...\n");
		payload_len=0;
		memset(buf,0,sizeof(buf));
		memset(&their_addr,0,sizeof(their_addr));
		memset(destination_ip,0,sizeof(destination_ip));
		addr_len = sizeof their_addr;
		
		if ((numbytes = recvfrom(sockfd, buf, mss , 0,
			(struct sockaddr *)&their_addr, &addr_len)) == -1) {
			perror("recvfrom");
			exit(1);
		}
		printf("Read bytes %d\n",numbytes);
		current_probability = (float)rand()/ (float)RAND_MAX;
		printf("Random float is %f and error value %f\n",current_probability,error_probablity);
		if ( current_probability <= error_probablity ){
			printf("************** Discard packet with error probability *************** %f <= %f \n",current_probability,error_probablity);
			continue;
		}
		 
		strncpy(destination_ip,inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),s, sizeof s),strlen(inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),s, sizeof s)));
//		printf("listener: packet is %d bytes long from destination %s\n", numbytes,destination_ip);
		buf[numbytes] = '\0';
//		printf("listener: packet contains \"%s\"\n", buf);

		memcpy(&recv_packet_sequence_number,&buf[0],sizeof(recv_packet_sequence_number));
		memcpy(&recv_packet_checksum,&buf[4],sizeof(recv_packet_checksum));
		memcpy(&recv_packet_msg_type,&buf[6],sizeof(recv_packet_msg_type));
		printf("-------  Got file data : %x %x\n",buf[8],buf[9]);
		fwrite(&buf[8],sizeof(char),numbytes - OTHER_HEADER,fp);
		recv_packet_sequence_number=ntohl(recv_packet_sequence_number);
		recv_packet_checksum=ntohs(recv_packet_checksum);
		recv_packet_msg_type=ntohs(recv_packet_msg_type);
		printf("Got sequence num : %d , checksum :%d and msg_type %u\n",(int)recv_packet_sequence_number,(int)recv_packet_checksum,(unsigned short)recv_packet_msg_type);
	//	If received data, verify checksum 
		if(verify_checksum(recv_packet_checksum,buf,numbytes) == 1){
			printf(" --------------- Checksum mismatch ----------------\n" );
			continue;
		}
	//	If checksum wrong discard the packet
		int32_t sequence_number,ack_sequence_number;
		sequence_number=recv_packet_sequence_number;
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
//		printf("Got sequence num : %d and i expected value %d\n", sequence_number,expected_sequence_number);
	//	Check for sequence number..
		memset(buf,0,sizeof(buf));
	//	snprintf(buf,sizeof(buf),"%d",ack_sequence_number);
		packet_sequence_number=htonl(ack_sequence_number);
		packet_msg_type=htons(0b1010101010101010);
		payload_len=8;
		//snprintf(&buf[0],sizeof(buf),"%d",sent_sequence_num);
		memcpy(&buf[0],(void *)&packet_sequence_number,sizeof(packet_sequence_number));
		memcpy(&buf[6],(void *)&packet_msg_type,sizeof(packet_msg_type));
		packet_checksum=generate_checksum(buf,payload_len);
	//	If sequence number is wrong, send ACK for required packet
		if ((numbytes = sendto(sockfd, buf, payload_len+1, 0,(struct sockaddr *)&their_addr, addr_len)) == -1) {
			perror("talker: sendto");
		}	
	
		printf("--------------------   ASKING : %d  ------------  and %d \n\n",ack_sequence_number,numbytes);
	//	Send the data to the application 
//		printf("Got correct sequence number\n");

//	TNRC : Change 1 to size of data 
	}
    	close(sockfd);
	return 0;
}

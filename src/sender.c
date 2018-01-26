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
#include "../include/linklist.h"
#include "../include/reliable_sender.h"
#include <fcntl.h>
#include<time.h>
//#include "../include/reliable_udp.h"

list clients;
#define SENDER_PORT "65432"
#define RECEIVER_PORT "7736"
#define OTHER_ARGUMENTS 3
#define HEADER_SIZE 8
#define SECOND_TIMEOUT 0
#define MICROSECOND_TIMEOUT 750000
int mss=1500;
int sockfd;
char buf[2048]={0};
char recv_buf[2048]={0};
int payload_len=0;
struct timeval result={SECOND_TIMEOUT,MICROSECOND_TIMEOUT};
uint16_t ones_complement=0b1111111111111111;
//	Write the data into the file from sender.

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

uint16_t generate_checksum(char *buf){
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


// TNRC : This line is the problem 


	if( payload_len % 2 == 0 ){
		printf("Padding not needed\n");
		end_ptr=(uint16_t *)&buf[payload_len];
	}
	else{
//		printf("Adding the padding bit in the mix\n");
		padded_end_ptr=(char* )&buf[payload_len];
		padded_end_ptr++;
		end_ptr=(uint16_t *)padded_end_ptr;	
	}
	while ( checksum_ptr != end_ptr){
//		printf("Call to add 16 bits \n");
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
int verify_checksum(int32_t recv_packet_checksum,char *buf){
	uint16_t new_checksum=generate_checksum(buf);
	if( recv_packet_checksum ^ new_checksum == ones_complement){
		return 0;
	}
	return 1;
}


bool client_traversal(void *this_client_info){
        printf(" Client  IP :%s\n",(((client_data*)this_client_info)->IPADDR));
        return TRUE;
}

bool send_udp_packet(void * this_client_info){
	int numbytes=0;
	uint16_t checksum=0;
	if ( ! ((client_data *)this_client_info)->got_ack ){
		checksum=generate_checksum(buf);
		printf("Buf strlen : %d.... %x %x %x %x %x %x %x %x %x %x\n",payload_len,buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9]);
		numbytes=sendto(sockfd,(char*)buf,payload_len,0,(struct sockaddr *)&(((client_data *)this_client_info)->sock_addr),sizeof(struct sockaddr));
		if( numbytes == -1 ){
			perror("Sendto :");
		}
		printf("I sent : %d bytes \n",numbytes);
	}
	else {
		printf("This destination already has IP.. Skipping it\n");
	}
	return TRUE;
}
bool update_ack(void * this_client_info, char *destination_ip){
	if(!strcmp(destination_ip,((client_data *)this_client_info)->IPADDR)){
		printf("Destination IP %s matches with clinet list IP \n",destination_ip);
		((client_data *)this_client_info)->got_ack=1;
	}
	return TRUE;
}
bool clear_acks(void * this_client_info){
	((client_data *)this_client_info)->got_ack=0;
	return TRUE;
}

bool is_ack_done(void * this_client_info){
	if (((client_data *)this_client_info)->got_ack )
		return TRUE;
	return FALSE;
}

int main(int argc, char *argv[]){
	if( argc < 3 ){
		printf("Insufficient arguments .. Destination IP and MSS need but got total values %d\n",argc -1);
		exit(1);
	}
// 	Create a socket
	char destination_ip[20]={0};
	struct addrinfo hints,*p,*servinfo;
	struct sockaddr_storage their_addr;
    	socklen_t addr_len;
    	char s[INET6_ADDRSTRLEN];
	int rv,numbytes=0;
	int32_t expected_sequence_number=0;
	fd_set master;
	int fd_max;
 	struct timeval tv;
	FILE *fp;
	struct timespec tstart={0,0}, tend={0,0};
	int temp_time_diff=0;

	fp=fopen(argv[2],"r+");
	if( !fp ){
		printf("Cannot open file %s for sending\n",argv[2]);
		exit(1);
	}
        tv.tv_usec = result.tv_usec;
        tv.tv_sec = result.tv_sec;
	payload_len=0;
	memset(&hints,0,sizeof(hints));
	hints.ai_family=AF_INET;
	hints.ai_socktype=SOCK_DGRAM;
	mss=atoi(argv[1])-1;
	int total_clients = argc - OTHER_ARGUMENTS,temp=0;
	struct client_data cd;
	list_new(&clients,sizeof(client_data),NULL);
	for(temp=0;temp<total_clients;temp++){
		memset(&cd,0,sizeof(client_data));
		strncpy(&cd.IPADDR[0],argv[temp + OTHER_ARGUMENTS],IPV4_ADDRLEN);
		cd.sock_addr.sin_family=AF_INET;
		cd.sock_addr.sin_port=htons(atoi(RECEIVER_PORT));
		inet_pton(AF_INET,cd.IPADDR,&cd.sock_addr.sin_addr.s_addr);
		list_append(&clients, (void *)&cd);
	}
	list_for_each(&clients,client_traversal);
	if ((rv = getaddrinfo(argv[1], RECEIVER_PORT, &hints, &servinfo)) != 0) {
        	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        	return 1;
    	}
	for ( p = servinfo ; p!= NULL ; p = p->ai_next){
		if((sockfd=socket(p->ai_family,p->ai_socktype, p->ai_protocol))==-1){
			close(sockfd);
			perror("socket :");
			continue;
		}/*
		if ( bind(sockfd,p->ai_addr,p->ai_addrlen) == -1){
			perror("Bind :");
			close(sockfd);
			continue;
		}*/
		break;
	}	
	if ( p == NULL ){
		fprintf(stderr, "listener: failed to bind socket\n");
        	return 2;
	}
	freeaddrinfo(servinfo);
//	Check for Data on the socket
	int yes=0,retval,recv_done=0;
	fd_max=sockfd;
	int32_t packet_sequence_number=0,recv_packet_sequence_number=0;
	int16_t packet_checksum=0,recv_packet_checksum=0;
	uint16_t packet_msg_type=0,recv_packet_msg_type=0;
	int sent_sequence_num=0,received_ack_num=0,sent_bytes=-1;
	size_t len;
	unsigned long long totaltime=0,mytime=0;
	int recv_complete=0;
	int status = fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);
	if (status == -1){
		perror("fcntl:");
	}


        struct timespec tstart_timer={0,0};
        clock_gettime(CLOCK_MONOTONIC, &tstart_timer);
        unsigned long long start_time_in_micros = 1000000000 * tstart_timer.tv_sec + tstart_timer.tv_nsec;
//      printf("Download start time : %lld \n",start_time_in_micros);



	mytime=result.tv_sec*1000000 + result.tv_usec;
	memset(&buf[0],0,sizeof(buf));
	payload_len=0;
	payload_len=8;
	len=fread(&buf[8],sizeof(char), mss - 8 ,fp);
	payload_len=payload_len+len;
	if( len == 0){
		printf(" **************  File transfer complete ***************\n");
		fclose(fp);
		close(sockfd);
		exit(1);
	}
	printf("Filegave %d\n",(int)len);

	while(1){
//		while ( received_ack_num != sent_sequence_num + 1 ){
			printf("My ack val : %d and current received ack value %d\n",sent_sequence_num,received_ack_num);
			memset(&destination_ip,0,sizeof(destination_ip));
			printf("I want to send %d\n",sent_sequence_num);
			packet_sequence_number=htonl(sent_sequence_num);
			packet_msg_type=htons(0b0101010101010101);
			memcpy((void *)&buf[0],(void *)&packet_sequence_number,sizeof(packet_sequence_number));
			memcpy((void *)&buf[6],(void *)&packet_msg_type,sizeof(packet_msg_type));
			//snprintf(&buf[0],sizeof(buf),"%d",sent_sequence_num);
			list_for_each(&clients,send_udp_packet);
			FD_ZERO(&master);
 			FD_SET(sockfd, &master);
			while (1){
				clock_gettime(CLOCK_MONOTONIC, &tstart);
				retval = select(fd_max+1,&master,NULL,NULL,&tv);
				if (retval == -1){
					perror("select:");
					continue;
				}
				else if (retval){

					while(1){
						addr_len = sizeof their_addr;
						memset(&recv_buf,0,sizeof(recv_buf));
						printf(" **********************  Reading sent ack now ***********  \n");
						if ((numbytes = recvfrom(sockfd, recv_buf, mss , 0,(struct sockaddr *)&their_addr, &addr_len)) == -1) {
							printf("Recv complete... \n");
							break;
						}
						printf("Read bytes :%d\n",numbytes);
						if( numbytes > 0 ){
							snprintf(destination_ip,sizeof(destination_ip),"%s",inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),s, sizeof s));
							recv_buf[numbytes] = '\0';
							memcpy(&recv_packet_sequence_number,&recv_buf[0],sizeof(recv_packet_sequence_number));
							memcpy(&recv_packet_checksum,&recv_buf[4],sizeof(recv_packet_checksum));
							memcpy(&recv_packet_msg_type,&recv_buf[6],sizeof(recv_packet_msg_type));
							recv_packet_sequence_number=ntohl(recv_packet_sequence_number);
							recv_packet_checksum=ntohs(recv_packet_checksum);
							recv_packet_msg_type=ntohs(recv_packet_msg_type);
							printf("Got sequence num : %d , checksum :%d and msg_type %u\n",(int)recv_packet_sequence_number,(int)recv_packet_checksum,(unsigned short )recv_packet_msg_type);
						//	If received data, verify checksum 
							if(verify_checksum(recv_packet_checksum,recv_buf) == 1){
								continue;
							}
							memset(&tv,0,sizeof(tv));	
							clock_gettime(CLOCK_MONOTONIC, &tend);
							tend.tv_sec= tend.tv_sec - tstart.tv_sec;
							tend.tv_nsec = tend.tv_nsec - tstart.tv_nsec;	
							totaltime=( tend.tv_sec*1000000000 + tend.tv_nsec ) / 1000;
							totaltime = mytime - totaltime;
							tv.tv_usec = ( totaltime % 1000000 );
							tv.tv_sec = totaltime / 1000000;
						//	If checksum wrong discard the packet
							received_ack_num=recv_packet_sequence_number;
							printf(" Destination %s responded with sequence num : %d and I wanted %d\n", destination_ip,received_ack_num,sent_sequence_num + 1);
							if ( received_ack_num == sent_sequence_num + 1 ){
								printf("Updating ACK flag for the current sequence number for this destinatio IP\n");
								list_for_each_update_ack(&clients,update_ack,destination_ip);

								printf("Check if all clients have received the data ?\n");
								recv_done=list_each_check_recv_complete(&clients,is_ack_done);	
								if( recv_done != 0 )
									continue;
								printf("Clearing all ACK flags and update next sequence number value\n");
								list_for_each(&clients,clear_acks);
								sent_sequence_num+=1;
								recv_complete=1;
								memset(&buf[0],0,sizeof(buf));
								payload_len=8;
								len=fread(&buf[8],sizeof(char), mss - 8 ,fp);
								payload_len=payload_len+len;
								if( len == 0){
									printf(" **************  File transfer complete ***************\n");
									struct timespec tend={0,0};
									clock_gettime(CLOCK_MONOTONIC, &tend);
									unsigned long long end_time_in_micros = 1000000000 * tend.tv_sec + tend.tv_nsec;
								//      printf("Download complete time : %lld \n",end_time_in_micros);

									printf("Time delta : %lld\n",end_time_in_micros-start_time_in_micros);
									fclose(fp);
									close(sockfd);
									exit(1);
								}
							}
						}
					}
					if ( recv_complete == 1 ){
						recv_complete = 0;
						break;
					}
				}
				else {
					memset(&tv,0,sizeof(tv));	
					tv.tv_usec = result.tv_usec;
					tv.tv_sec = result.tv_sec;
					if ( recv_complete == 1 ){
						recv_complete = 0;
						break;
					}
					printf("*************  TIMER has expired... resending the packets ******************** \n");
					break;	
				}
			}
		//	Check for sequence number..
		//	If sequence number is wrong, send ACK for required packet
		
//		}
	}
    	close(sockfd);
	return 0;
}

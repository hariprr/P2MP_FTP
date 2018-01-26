#define IPV4_ADDRLEN 20
typedef struct client_data{
	char IPADDR[IPV4_ADDRLEN];
	int got_ack;
	struct sockaddr_in sock_addr;
}client_data;

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>


#include "dns_server.h"
#include "dns.h"

struct dns_server *dns_server_new(char *address, unsigned short int port) {

	struct dns_server *srv;
	
	srv = (struct dns_server *)malloc(sizeof(struct dns_server));
	
	if (srv == NULL)
		return NULL;
	
	if (inet_pton(AF_INET, address, (void *)&srv->inet_address.sin_addr) == 0)
		return NULL;
		
	srv->inet_address.sin_port = htons(port);
	srv->inet_address.sin_family = AF_INET;
	
	strncpy(srv->hr_address, address, sizeof(srv->hr_address));
	srv->hr_port = port;
	
	return srv;
	
}

void dns_server_destroy(struct dns_server *srv) {
	
	free(srv);
	
}

int dns_server_resolve(struct dns_server *srv, int socket, struct dns_data *data, unsigned int data_len) {
	
	int res;
	unsigned int salen = sizeof(srv->inet_address);
	
	res = sendto(socket, data, data_len, 0, (struct sockaddr *)&srv->inet_address, salen);
	if (res == 0) {
		fprintf(stderr, "Could not send data\n");
		return 0;
	}
			
	res = recvfrom(socket, data, sizeof(struct dns_data), 0, (struct sockaddr *)&srv->inet_address, &salen);
	if (res == -1) {
		fprintf(stderr, "Could not receive data back. Timeout?\n");
		return 0;
	}
	
	return res;
	
}

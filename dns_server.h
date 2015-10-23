#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "dns.h"

#ifndef DNS_SERVER_H
#define DNS_SERVER_H

struct dns_server {
	struct sockaddr_in inet_address;
	char hr_address[32];
	unsigned int hr_port;
};

struct dns_server *dns_server_new(char *, unsigned short int);
void dns_server_destroy(struct dns_server *);
int dns_server_resolve(struct dns_server *, int, struct dns_data *, unsigned int);

#endif

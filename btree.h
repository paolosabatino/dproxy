#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "dns.h"

#ifndef BTREE_H
#define BTREE_H

struct payload {
	char host[DNS_NAME_SIZE];
	unsigned short int type;
	struct dns_data buffer;
	unsigned short int buf_len;
	unsigned int expires;
};

struct node {
    struct payload payload;
    struct node *left;
    struct node *right;
};

void btinsert(struct node **, char *, unsigned short int, unsigned int, void *, unsigned short int);
struct node *btsearch(struct node *, char *, unsigned int);
void btdestroy(struct node *);
void btdelete(struct node **, char *, unsigned short int);
int btcount (struct node *);
void btprint (struct node *);
struct node *btbalance (struct node *);
void btprune (struct node **, unsigned int);
int btdepth (struct node *);

#endif

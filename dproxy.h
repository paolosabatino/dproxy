/*
  **
  ** dproxy.h
  **
  ** Part of the drpoxy package by Matthew Pratt.
  **
  ** Copyright 1999 Matthew Pratt <mattpratt@yahoo.com>
  **
  ** This software is licensed under the terms of the GNU General
  ** Public License (GPL). Please see the file COPYING for details.
  **
  **
*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <syslog.h>
#include <errno.h>

#include "dns.h"
#include "dns_server.h"
#include "btree.h"

#ifndef DPROXY_H
#define DPROXY_H

#ifndef PORT
#define PORT 53
#endif



#ifndef CONFIG_FILE_DEFAULT
#define CONFIG_FILE_DEFAULT "/etc/dproxy.conf"
#endif
#ifndef DENY_FILE_DEFAULT
#define DENY_FILE_DEFAULT "/etc/dproxy.deny"
#endif
#ifndef CACHE_FILE_DEFAULT
#define CACHE_FILE_DEFAULT "/var/cache/dproxy.cache"
#endif
#ifndef HOSTS_FILE_DEFAULT
#define HOSTS_FILE_DEFAULT "/etc/hosts"
#endif
#ifndef PURGE_TIME_DEFAULT
#define PURGE_TIME_DEFAULT 1 * 60 * 60
#endif
#ifndef PPP_DEV_DEFAULT
#define PPP_DEV_DEFAULT "/var/run/ppp0.pid"
#endif
#ifndef DHCP_LEASES_DEFAULT
#define DHCP_LEASES_DEFAULT "/var/state/dhcp/dhcpd.leases"
#endif
#ifndef PPP_DETECT_DEFAULT
#define PPP_DETECT_DEFAULT 1
#endif
#ifndef DEBUG_FILE_DEFAULT
#define DEBUG_FILE_DEFAULT "/var/log/dproxy.debug.log"
#endif
#ifndef WORKER_THREADS_COUNT_DEFAULT
#define WORKER_THREADS_COUNT_DEFAULT 1
#endif

struct cache *cache;
struct dns_server *server;

struct udp_packet {
	struct dns_data dns_data;
	unsigned short int dns_data_len;
	struct dns_cooked_header dns_chdr;
	struct in_addr src_ip;
	int src_port;
};

struct thread_info {
	pthread_t tid;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	struct udp_packet *pkt;
	int busy;
	int run;
};

struct addrinfo af_inet_hints;

void debug(char *fmt, ...);
void debug_perror(char * msg);

#endif

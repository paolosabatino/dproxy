/*
  **
  ** dproxy.c
  **
  ** Part of the dproxy package by Matthew Pratt.
  **
  ** Copyright 1999 Matthew Pratt <mattpratt@yahoo.com>
  **
  ** This software is licensed under the terms of the GNU General
  ** Public License (GPL). Please see the file COPYING for details.
  **
  **
*/

/*
  **
  ** See ChangeLog for version history.
  **
*/

#include <time.h>
#include <pthread.h>
#include "dproxy.h"
#include "cache.h"
#include "conf.h"
#include "dns.h"

#define MIN_SLEEP_TIMEOUT 10000;
#define MAX_SLEEP_TIMEOUT 1000000

/*****************************************************************************/
/* Global variables */
/*****************************************************************************/
/* function protos */
void usage(char * program , char * message );
int udp_sock_open(struct in_addr ip, int port);
int udp_packet_read(int sockfd, struct udp_packet *udp_pkt);
void *thread_resolve(void *args);
struct thread_info **create_worker_threads(unsigned int);
void stop_worker_threads(struct thread_info **, unsigned int);
struct dns_server *get_system_dns(void);

void sig_hup (int signo);
void sig_int (int);
void sig_usr1 (int);
void sig_usr2 (int);
int get_options( int argc, char ** argv );

int sockfd;
int run_process;

/*****************************************************************************/
int main(int argc, char **argv) {

	struct in_addr ip;
	int numread;
	struct thread_info **t_info;
	struct thread_info *target_thread;
	unsigned int t_cursor = 0;
	unsigned int t_idx;
	unsigned int sleep_timeout = MIN_SLEEP_TIMEOUT;
	
	unsigned int last_cache_purge = time(NULL);
	
	/* get commandline options, load config if needed. */
	if(get_options( argc, argv ) < 0 ) {
		exit(1);
	}

	ip.s_addr = INADDR_ANY;
	sockfd = udp_sock_open( ip, PORT );

	if (config.daemon_mode) {
		/* Standard fork and background code */
		switch (fork()) {
		case -1:	/* Oh shit, something went wrong */
			debug_perror("fork");
			exit(-1);
		case 0:	/* Child: close off stdout, stdin and stderr */
			close(0);
			close(1);
			close(2);
			break;
		default:	/* Parent: Just exit */
			exit(0);
		}
	}

	//signal(SIGHUP, sig_hup);
	signal(SIGINT, sig_int);
	signal(SIGTERM, sig_int);
	signal(SIGUSR1, sig_usr1);
	signal(SIGUSR2, sig_usr2);

	t_info = create_worker_threads(config.worker_threads_count);
	
	/*
	 * Instantiate a cache
	 */
	cache = cache_new();
	
	/*
	 * Instantiate a DNS remote server
	 */
	server = get_system_dns();
	
	if (server == NULL) {
		fprintf (stderr, "No DNS resolver available. Check /etc/resolv.conf for a valid DNS server\n");
		return 1;
	}

	/*
	 * Populate af_inet_hints global struct. We don't need to generate
	 * this each time a dns query is forwarded. We create the right
	 * structure once and use it for all the thread requests
	 */
	memset (&af_inet_hints, 0, sizeof(struct addrinfo));
	af_inet_hints.ai_family = AF_INET;
	af_inet_hints.ai_socktype = SOCK_DGRAM;
	af_inet_hints.ai_flags = 0;
	af_inet_hints.ai_protocol = 0;

	run_process = 1;

	while(run_process) {

		/*
		 * Find the next non-busy thread in the list
		 */
		target_thread = NULL;

		while (run_process) {

			t_cursor = (t_cursor + 1) % config.worker_threads_count;

			for (t_idx = 0; t_idx < config.worker_threads_count; t_idx++) {
				if (t_info[t_cursor]->busy == 0) {
					target_thread = t_info[t_cursor];
					debug ("Entering thread %x...\n", target_thread->tid);
					pthread_mutex_lock (&target_thread->mutex);
					target_thread->busy = 1;
					debug("Entered thread %x and set to busy\n", target_thread->tid);
					break;
				}
			}

			/*
			 * No threads are available, wait for some time and retry
			 */
			if (target_thread == NULL) {
				debug ("All threads are busy, waiting for %d microseconds...\n", sleep_timeout);
				usleep (sleep_timeout);
				sleep_timeout<<=1;
				if (sleep_timeout > MAX_SLEEP_TIMEOUT)
					sleep_timeout = MAX_SLEEP_TIMEOUT;
			} else {
				sleep_timeout=MIN_SLEEP_TIMEOUT;
				break;
			}

		}
		
		if ( time(NULL) > last_cache_purge + config.purge_time ) {
			printf ("Beginning cache tree tidying up (%d nodes)\n", cache_count(cache));
			cache_tidyup(cache, time(NULL));
			printf ("Tidying up complete, remaining %d nodes\n", cache_count(cache));
			last_cache_purge = (time(NULL));
		}

		debug("Next request will be assigned to thread %x\n", target_thread->tid);
		numread = udp_packet_read( sockfd, target_thread->pkt );
		if( numread < 0 ) {
			debug("no data ...\n");
			pthread_mutex_unlock(&target_thread->mutex);
			continue;
		}

		if(numread < sizeof(struct dns_header)+1 ) {
			debug("got packet with invalid size of %d \n",numread);
			pthread_mutex_unlock(&target_thread->mutex);
			continue;
		}

		//debug("Dns query from %s port %d\n", inet_ntoa(target_thread->pkt->src_ip), target_thread->pkt->src_port);

		/*
		 * Signal the thread that it has a job to do
		 */
		pthread_cond_signal (&target_thread->cond);
		pthread_mutex_unlock (&target_thread->mutex);

	}

	stop_worker_threads(t_info, config.worker_threads_count);
	cache_destroy(cache);
	dns_server_destroy(server);

	return 0;

}

/**
 * Creates a number of worker thread and fills a list of thread_info
 * structures
 */
struct thread_info **create_worker_threads(unsigned int count) {

	struct thread_info **t_info = malloc(sizeof(struct thread_info *) * count);
	unsigned int idx;

	debug ("Creating %d worker threads", count);

	for (idx = 0; idx < count; idx++) {
		t_info[idx] = malloc(sizeof(struct thread_info));
		t_info[idx]->tid=0;
		t_info[idx]->pkt=(struct udp_packet *)malloc(sizeof(struct udp_packet));
		t_info[idx]->busy=0;
		t_info[idx]->run=1;
		pthread_cond_init(&t_info[idx]->cond, NULL);
		pthread_mutex_init(&t_info[idx]->mutex, NULL);
		pthread_create(&t_info[idx]->tid, NULL, thread_resolve, t_info[idx]);
		debug("Created thread %d with tid %x\n", idx, t_info[idx]->tid);
	}

	return t_info;

}

/**
 * Stops all worker threads and wait for their termination
 */
void stop_worker_threads(struct thread_info **t_info, unsigned int count) {

	unsigned int idx;

	/*
	 * Assign the run variable to 0 for all the worker threads
	 */
	for (idx = 0; idx < count; idx++)
		t_info[idx]->run = 0;

	/*
	 * Then signal the condition variable over all the threads, then
	 * wait for them to join
	 */
	for (idx = 0; idx < count; idx++) {
		pthread_mutex_lock (&t_info[idx]->mutex);
		pthread_cond_signal (&t_info[idx]->cond);
		pthread_mutex_unlock (&t_info[idx]->mutex);
	}

	/*
	 * Finally wait for them to join
	 */
	for (idx = 0; idx < count; idx++) {
		pthread_join (t_info[idx]->tid, NULL);
	}

	/*
	 * And at last free the used memory for packets and destroy pthread
	 * resources
	 */
	for (idx = 0; idx < count; idx++) {
		free (t_info[idx]->pkt);
		pthread_mutex_destroy (&t_info[idx]->mutex);
		pthread_cond_destroy(&t_info[idx]->cond);
	}

	return;

}

void *thread_resolve (void *args) {

	int socket;
	unsigned short data_len;
	
	/*
	 * Thread private data
	 */
	struct thread_info *t_info;
	struct udp_packet *pkt;
	
	t_info=(struct thread_info *)args;
	pkt = t_info->pkt;
	
	/*
	 * Destination addresses
	 */
	struct sockaddr_in dst_sa;
	unsigned int dst_salen;
	
	memset((void *)&dst_sa, 0, sizeof(dst_sa));
	
	/*
	 * Generate the dns_sections static structures
	 */

	char query_host[DNS_NAME_SIZE];
	unsigned short int type;
	unsigned short int class;
	int in_cache;
	short int msg_id;
	int res;
		
	/*
	 * Open the listening socket
	 */
	struct in_addr lst_ip;
	unsigned int lst_salen;
	char lst_text_addr[16];
	struct sockaddr_in lst_addr;
	struct timeval tv;

	lst_ip.s_addr = INADDR_ANY;
	socket = udp_sock_open(lst_ip, 0);
	tv.tv_sec = 1; // Socket timeout
	tv.tv_usec = 0;
	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
	lst_salen = sizeof(struct sockaddr_in);
	getsockname(socket, (struct sockaddr *)&lst_addr, &lst_salen);
	inet_ntop (lst_addr.sin_family, &lst_addr.sin_addr, lst_text_addr, sizeof(lst_text_addr) );
	
	debug ("Socket %d opened with family %d address %s and port %d\n", socket, ntohs(lst_addr.sin_family), lst_text_addr, ntohs(lst_addr.sin_port));
	

	while (t_info->run == 1) {

		pthread_mutex_lock(&t_info->mutex);
		t_info->busy = 0;
		pthread_cond_wait(&t_info->cond, &t_info->mutex);

		if (t_info->run == 0) {
			pthread_mutex_unlock(&t_info->mutex);
			break;
		}
		
		/*
		 * If query bit is set to 1, it is not query, so we skip
		 * the resolution. We can't handle that packet.
		 */
		if (pkt->dns_chdr.query_bit != 0)
			continue;
			
		msg_id = pkt->dns_data.dns_hdr.dns_id;
			
		/*
		 * In case we have multiple query sections, we just skip the 
		 * caching and send the DNS packet to the main server. We can't
		 * handle multiple queries inside a single DNS request
		 */
		if (ntohs(pkt->dns_data.dns_hdr.dns_no_questions) == 1) {
			
			res = extract_request(pkt->dns_data.buf, query_host, &type, &class);
			/*
			 * Let's extract the name of the host, the type and the 
			 * class of the request. extract_request returns 1 if there
			 * is any problem, so we proceed to check the cache only
			 * if we get 0.
			 */
			 if (res == 0 && class == 1) {
				 
				 debug("Host: %s, type: %d class: %d\n", query_host, type, class);
				 
				 /*
				  * Search the packet in cache
				  */
				 in_cache = cache_search(cache, query_host, type, time(NULL), (void *)&pkt->dns_data, &data_len);
				 
				 if (in_cache == 1) {
					 /*
					  * Change the msg id to reflect the same msgid of the request
					  */
					 //debug ("Cache hit, found %d bytes\n", data_len);
					 pkt->dns_data.dns_hdr.dns_id = msg_id;
				 }
				 
				 
			 } else {
				 
				 debug("Extract request failed\n");
				 
			 }
				 
		}
		
		/* 
		 * If the query is not in cache, ask a server to resolve the DNS query
		 * and then cache the response
		 */
		if (in_cache == 0) {
			
			//debug ("Packet not in cache, resolving with server...\n");
			data_len = dns_server_resolve(server, socket, &pkt->dns_data, pkt->dns_data_len);
			
			if (data_len > 0 && msg_id == pkt->dns_data.dns_hdr.dns_id && class == 1) {
				cache_insert(cache, query_host, type, time(NULL) + config.purge_time, (void *)&pkt->dns_data, data_len);	
				//debug("Cached %d bytes\n", data_len);
			}
			
		}
		
		if (data_len > 0 && msg_id == pkt->dns_data.dns_hdr.dns_id) {
			
			dst_sa.sin_addr = pkt->src_ip;
			dst_sa.sin_port = htons(pkt->src_port);
			dst_sa.sin_family = AF_INET;
			dst_salen = sizeof(dst_sa);
			
			data_len = sendto(sockfd, &pkt->dns_data, data_len, 0, (struct sockaddr *)&dst_sa, dst_salen);

		}

		pthread_mutex_unlock(&t_info->mutex);

		debug("Done.\n");

	}

	debug("Thread %x terminated\n", t_info->tid);
	pthread_exit(NULL);

}

/*****************************************************************************/
int udp_sock_open(struct in_addr ip, int port) {
	int fd;
	struct sockaddr_in sa;

	/* Clear it out */
	memset((void *)&sa, 0, sizeof(sa));

	fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	/* Error */
	if( fd < 0 ) {
		debug_perror("Could not create socket");
		exit(1);
	}

	sa.sin_family = AF_INET;
	memcpy((void *)&sa.sin_addr, (void *)&ip, sizeof(struct in_addr));
	sa.sin_port = htons(port);

	/* bind() the socket to the interface */
	if (bind(fd, (struct sockaddr *)&sa, sizeof(struct sockaddr)) < 0) {
		debug_perror("Could not bind to port");
		exit(1);
	}

	return(fd);
}


/*****************************************************************************/
int udp_packet_read(int sockfd, struct udp_packet *udp_pkt ) {
	
	int numread;
	
	struct sockaddr_in sa;
	unsigned int salen;
	unsigned short flags;

	/* Read in the actual packet */
	salen = sizeof(sa);
	numread = recvfrom(sockfd, &udp_pkt->dns_data, sizeof(struct dns_data), 0, (struct sockaddr *)&sa, &salen);
	udp_pkt->dns_data_len = numread;

	if (numread < 0) {
		debug_perror("udp_read_read: recvfrom");
		debug("udp_read_read: recvfrom\n");
		return -1;
	}
	
	flags = ntohs(udp_pkt->dns_data.dns_hdr.dns_flags);
	
	udp_pkt->dns_chdr.message_id = ntohs(udp_pkt->dns_data.dns_hdr.dns_id);
	udp_pkt->dns_chdr.query_bit = flags & 0x01;
	udp_pkt->dns_chdr.opcode = (flags & 0x1e) >> 1;
	udp_pkt->dns_chdr.auth_answer = (flags & 0x20) >> 5;
	udp_pkt->dns_chdr.truncated = (flags & 0x40) >> 6;
	udp_pkt->dns_chdr.recurse_desired = (flags & 0x80) >> 7;
	udp_pkt->dns_chdr.recurse_avail = (flags & 0x100) >> 8;
	udp_pkt->dns_chdr.z_field = (flags & 0xe00) >> 9;
	udp_pkt->dns_chdr.rcode = (flags & 0xf000) >> 12;
	

	/* Then record where the packet came from */
	memcpy((void *)&udp_pkt->src_ip, (void *)&sa.sin_addr, sizeof(struct in_addr));
	udp_pkt->src_port = ntohs(sa.sin_port);
	
	return numread;
}

/**
 * Seek into /etc/resolv.conf the first available dns server to use as 
 * remote server
 */
struct dns_server *get_system_dns(void) {
	
	FILE *f_in;
	char row[64];
	char address[64];
	int ret;
	struct dns_server *dns_server = NULL;
	
	f_in = fopen("/etc/resolv.conf","rt");
	
	if (f_in == NULL)
		return NULL;
		
	while (fgets(row, sizeof(row), f_in)) {
		ret = sscanf(row, "nameserver %s\n", address);
		/*
		 * Do some minimal filtering over the nameserver. We don't want
		 * localhost as this will cause a loop
		 */
		if (ret == 1 && 
			strcmp(address, "127.0.0.1") != 0 && 
			strcmp(address, "127.0.1.1") != 0 &&
			strcmp(address, "localhost") != 0) {
			dns_server = dns_server_new(address, 53);
			if (dns_server != NULL)
				break;
		}
	}
	
	debug ("Using dns server %s:53 from /etc/resolv.conf\n", address);
	
	return dns_server;
	
}

/*****************************************************************************
 * same as debug_perror but writes to debug output.
 *
*****************************************************************************/
void debug_perror( char * msg ) {
	debug( "%s : %s\n" , msg , strerror(errno) );
}
/*****************************************************************************/
void debug(char *fmt, ...) {
#define MAX_MESG_LEN 1024

	va_list args;
	char text[ MAX_MESG_LEN ];

	sprintf( text, "[ %d ]: ", getpid());
	va_start (args, fmt);
	vsnprintf( &text[strlen(text)], MAX_MESG_LEN - strlen(text), fmt, args);
	va_end (args);

	if( config.debug_file[0] ) {
		FILE *fp;
		fp = fopen( config.debug_file, "a");
		if(!fp) {
			syslog( LOG_ERR, "could not open log file %m" );
			return;
		}
		fprintf( fp, "%s", text);
		fclose(fp);
	}

	/** if not in daemon-mode stderr was not closed, use it. */
	if( ! config.daemon_mode ) {
		fprintf( stderr, "%s", text);
	}

}
/*****************************************************************************/

void sig_hup (int signo) {
	//signal(SIGHUP, sig_hup); /* set this for the next sighup */
	//conf_load (config.config_file);
}

void sig_int(int signo) {
	run_process = 0;
	close(sockfd);
}

void sig_usr1(int signo) {
	printf ("Cached domain list:\n");
	cache_print (cache);
}

void sig_usr2(int signo) {
	printf ("Beginning cache tree pruning (%d nodes)\n", cache_count(cache));
	cache_tidyup(cache, time(NULL));
	printf ("Pruning complete, remaining %d nodes\n", cache_count(cache));
}

/*****************************************************************************
 * print usage informations to stderr.
 *
 *****************************************************************************/
void usage(char * program , char * message ) {
	fprintf(stderr,"%s\n" , message );
	fprintf(stderr,"usage : %s [-c <config-file>] [-d] [-h] [-P]\n", program );
	fprintf(stderr,"\t-c <config-file>\tread configuration from <config-file>\n");
	fprintf(stderr,"\t-d \t\trun in debug (=non-daemon) mode.\n");
	fprintf(stderr,"\t-P \t\tprint configuration on stdout and exit.\n");
	fprintf(stderr,"\t-h \t\tthis message.\n");
}
/*****************************************************************************
 * get commandline options.
 *
 * @return 0 on success, < 0 on error.
 *****************************************************************************/
int get_options( int argc, char ** argv ) {
	char c = 0;
	int not_daemon = 0;
	int want_printout = 0;
	char * progname = argv[0];

	conf_defaults();

	while( (c = getopt( argc, argv, "c:dhP")) != EOF ) {
		switch(c) {
		case 'c':
			conf_load(optarg);
			break;
		case 'd':
			not_daemon = 1;
			break;
		case 'h':
			usage(progname,"");
			return -1;
		case 'P':
			want_printout = 1;
			break;
		default:
			usage(progname,"");
			return -1;
		}
	}

	/** unset daemon-mode if -d was given. */
	if( not_daemon ) {
		config.daemon_mode = 0;
	}

	if( want_printout ) {
		conf_print();
		return -1;
	}
	return 0;
}

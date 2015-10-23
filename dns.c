#include "dproxy.h"
#include "dns.h"

/** 
 * Queries are encoded such that there is and integer specifying how long
 * the next block of text will be before the actuall text. For eaxmple:
 *             www.linux.com => \03www\05linux\03com\0
 * This function replaces the query in the array with its dot '.' seperated
 * equivalent. 
 * 
 * Reimplemented by Paolo Sabatino with buffer overflow prevention.
 */
int decode_domain_name(char *query) {

	char decoded[DNS_NAME_SIZE];
	
	unsigned short int idx;
	unsigned short didx;
	unsigned short len;
	
	idx = 0;
	didx = 0;
	
	decoded[0] = 0;
	
	while( query[idx] && idx < DNS_NAME_SIZE ) {
		
		len = query[idx];
		idx++;
		
		/*
		 * Check if we are doing a buffer overflow. If so, return failure
		 */
		if (len + didx > DNS_NAME_SIZE)
			return 1;
			
		if (len + idx > DNS_NAME_SIZE)
			return 1;
			
		/*
		 * Copy the given characters into decoded string
		 */
		 for (; len > 0; len--) {
			 decoded[didx++] = query[idx++];
		 }
		 
		 /*
		  * Add a separating dot
		  */
		 decoded[didx++] = '.'; 
		
	}
	
	if ( didx > 0 )
		decoded[didx-1] = 0;
		
	strncpy (query, decoded, DNS_NAME_SIZE);
	
	return 0;
	
}

/**
 * This function encode the plain string in name to the domain name encoding
 * see decode_domain_name for more details on what this function does.
 * 
 * Reimplemented by Paolo Sabatino with buffer overflow prevention
 */
int encode_domain_name(char *query) {

	char encoded[DNS_NAME_SIZE];
	short idx;
	unsigned short len;
	unsigned short toklen;
	
	
	/*
	 * Check that the string is no longer than the buffer size - 1. We are
	 * going to need one more character than the string to encode.
	 */
	len = strnlen(query, DNS_NAME_SIZE); 
	if ( len >= DNS_NAME_SIZE + 1)
		return 1; // Failure because the string is longer than the buffer size
		
	// Copy the query string leaving the first byte of encoded empty.
	// We are going to put a dot there, that will be translated into
	// the token length
	strncpy(&encoded[1], query, DNS_NAME_SIZE - 1); 
	encoded[0] = '.';
	
	toklen = 0;
	
	/*
	 * Do the search backward, from the end of the string to the beginning.
	 * If we find a dot, replace with the non-dot character already read.
	 */
	for ( idx = len; idx >= 0; idx--) {
		if ( encoded[idx] == '.' ) {
			encoded[idx] = toklen;
			toklen = 0;
		} else {
			toklen++;
		}
		
	}
	
	strncpy (query, encoded, DNS_NAME_SIZE);
	
	return 0;
	
}

/** 
 * reverse lookups encode the IP in reverse, so we need to turn it around
 * example 2.0.168.192.in-addr.arpa => 192.168.0.2
 * this function only returns the first four numbers in the IP
 * NOTE: it assumes that name points to a buffer BUF_SIZE long
 * 
 * Reimplemented by Paolo Sabatino with buffer overflow prevention:
 * 
 * In-place reverses domain name. 
 * To be used only if in case of IPv4 reverse lookup.
 * Returns 0 if successful, 1 if address is not IPv4
 */
int reverse_domain_name(char *name) {
	
	int octet[4];
	int res;
	int idx;
	
	res = sscanf(name, "%d.%d.%d.%d", &octet[3], &octet[2], &octet[1], &octet[0]);
	
	for (idx = 0; idx < 4; idx++) {
		if (octet[idx] < 0 || octet[idx] > 255)
			return 1;
	}
	
	if (res == 4) {
		sprintf(name, "%d.%d.%d.%d", octet[0], octet[1], octet[2], octet[3]);
		return 0;
	}
	
	return 1;
		
}

int extract_request(char *buffer, char *host_name, unsigned short int *type, unsigned short int *class) {
	
	int res;
	int enl;
	
	/*
	 * Copy the string that is at most DNS_NAME_SIZE bytes into query_name
	 * buffer, then apply decode_domain_name that decodes the domain name
	 * inplace.
	 */
	strncpy (host_name, buffer, DNS_NAME_SIZE);
	res = decode_domain_name(host_name);
	if (res == 1) // decode_domain_name failed return with error code 1
		return 1;
	
	/*
	 * To get the type and class we have to find the end of the encoded
	 * DNS name string and grab the next two couple of bytes
	 */
	enl = strnlen(buffer, BUF_SIZE);
	if (enl == BUF_SIZE) // Can't find the end of the encoded DNS name
		return 1;
		
	(*type) = ntohs(*(unsigned short int*)(buffer+enl+1));
	(*class) = ntohs(*(unsigned short int*)(buffer+enl+3));
	
	return 0;
	
}

/**
 * Fills a supplied data structure with pointers to each section of the
 * DNS data buffer.
 * Returns the number of the structure actually stored in ptr_queries
 */
int fill_data_struct(struct dns_data *dns_data, struct dns_section **ptr_queries, unsigned int max) {
	
	unsigned int idx;
	unsigned int section = 0;
	char *buf_pointer = dns_data->buf;
	
	for (idx = 0; idx < ntohs(dns_data->dns_hdr.dns_no_questions); idx++) {
		
		ptr_queries[section]->ptr = (void *)buf_pointer;
		ptr_queries[section]->type = DNS_QUERY;
		buf_pointer += strlen(buf_pointer) + 4;
		section++;
		
	}
	
	for (idx = 0; idx < ntohs(dns_data->dns_hdr.dns_no_answers); idx++) {
		
		ptr_queries[section]->ptr = (void *)buf_pointer;
		ptr_queries[section]->type = DNS_ANSWER;
		buf_pointer += strlen(buf_pointer);
		buf_pointer += 10 + ntohs((unsigned short int)*(&buf_pointer[8]));
		section++;
		
	}
	
	for (idx = 0; idx < ntohs(dns_data->dns_hdr.dns_no_authority); idx++) {
		
		ptr_queries[section]->ptr = (void *)buf_pointer;
		ptr_queries[section]->type = DNS_AUTHORITATIVE;
		buf_pointer += strlen(buf_pointer);
		buf_pointer += 10 + ntohs((unsigned short int)*(&buf_pointer[8]));
		section++;
		
	}
	
	for (idx = 0; idx < ntohs(dns_data->dns_hdr.dns_no_additional); idx++) {
		
		ptr_queries[section]->ptr = (void *)buf_pointer;
		ptr_queries[section]->type = DNS_ADDITIONAL;
		buf_pointer += strlen(buf_pointer);
		buf_pointer += 10 + ntohs((unsigned short int)*(&buf_pointer[8]));
		section++;
		
	}
	
	return section;
	
}

void print_query (void *ptr) {
	char query_name[DNS_NAME_SIZE];
	unsigned short int query_type;
	unsigned short int query_class;
	
	printf ("Starting pointer: %d\n", (int)ptr);
	
	strncpy(query_name, ptr, sizeof(query_name));
	ptr += strlen(query_name) + 1;
	query_type = *((unsigned short int *)ptr);
	ptr+=2;
	query_class = *((unsigned short int *)ptr);
	
	decode_domain_name(query_name);
	
	printf ("Host: %s - query type: %d class: %d\n", query_name, ntohs(query_type), ntohs(query_class));
	
}

void print_resource (void *ptr) {
	
	char query_name[DNS_NAME_SIZE];
	char resource[DNS_NAME_SIZE];
	
	unsigned short int query_type;
	unsigned short int query_class;
	unsigned long int ttl;
	unsigned short int rdata_len;
	
	printf ("Starting pointer: %d\n", (int)ptr);
	
	inet_ntop(AF_INET, ptr, query_name, DNS_NAME_SIZE);
	//strncpy(query_name, ptr, sizeof(query_name));
	ptr += strlen((char *)ptr) + 1;
	query_type = ntohs(*((unsigned short int *)ptr));
	ptr+=2;
	query_class = ntohs(*((unsigned short int *)ptr));
	ptr+=2;
	ttl=ntohl(*((unsigned long int *)ptr));
	ptr+=4;
	rdata_len=ntohs(*((unsigned short int *)ptr));
	ptr+=2;
	memcpy(resource, ptr, rdata_len > sizeof(resource) ? sizeof(resource) : rdata_len);
	
	//decode_domain_name(query_name);
	
	printf ("Host: %s - query type: %u class: %u\n", query_name, query_type, query_class);
	printf ("TTL: %u - rdata len:%u - data: %s\n", ttl, rdata_len, resource);
	
}

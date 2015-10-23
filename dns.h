/*
  **
  ** dns.h - data structures used in dns queries and answers.
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

#ifndef DNS_H
#define DNS_H

#define MAX_DNS_SECTIONS 32
#define BUF_SIZE 536
#define DNS_NAME_SIZE 256

struct dns_header {
	short int dns_id;
	short int dns_flags;
	short int dns_no_questions;
	short int dns_no_answers;
	short int dns_no_authority;
	short int dns_no_additional;
};

struct dns_data {
	struct dns_header dns_hdr; /* This contains the HEADER part */
	char buf[BUF_SIZE]; /* This will contain all the QUERY and RESOURCE RECORDS */
};	


struct dns_query {
	char *query;
	unsigned short int type;
	unsigned short int class;
};

struct dns_resource_record {
	unsigned char *name;
	unsigned short int type;
	unsigned short int class;
	unsigned long int ttl;
	unsigned short int rdata_length;
	unsigned char *rdata;
};

struct dns_answer {
	short int dns_name;
	short int dns_type;
	short int dns_class;
	short int dns_time_to_live;
	short int dns_time_to_live2;
	short int dns_data_len;
};

enum DNS_SECTION_TYPE{
	DNS_QUERY = 0x0,
	DNS_ANSWER = 0x1,
	DNS_AUTHORITATIVE = 0x2,
	DNS_ADDITIONAL = 0x3
};

struct dns_section {
	enum DNS_SECTION_TYPE type;
	void *ptr;
};

struct dns_cooked_header {
	unsigned short int message_id;
	unsigned short int query_bit;
	unsigned short int opcode;
	unsigned short int auth_answer;
	unsigned short int truncated;
	unsigned short int recurse_desired;
	unsigned short int recurse_avail;
	unsigned short int z_field;
	unsigned short int rcode;
};

int fill_data_struct(struct dns_data *, struct dns_section **, unsigned int);
int encode_domain_name(char *name);
int decode_domain_name(char *name);
int reverse_domain_name(char name[BUF_SIZE]);
void print_query (void *ptr);
void print_resource (void *ptr);
int extract_request(char *, char *, unsigned short int *, unsigned short int *);

#endif
/* EOF */

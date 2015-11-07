#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "dproxy.h"
#include "btree.h"
#include "cache.h"

struct cache *cache_new () {
	
	struct cache *cache;
	
	cache = (struct cache *)malloc(sizeof(struct cache));
	
	cache->tree = NULL;
	pthread_rwlock_init (&cache->rwlock, NULL);
	
	return cache;
	
}

void cache_destroy (struct cache *cache) {

	if (cache == NULL);
		return;
		
	pthread_rwlock_wrlock(&cache->rwlock);
	
	btdestroy (cache->tree);
	cache->tree = NULL;
	
	pthread_rwlock_unlock(&cache->rwlock);
	
	pthread_rwlock_destroy(&cache->rwlock);
	
	free (cache);
	
}


int cache_search (struct cache *cache, char *host, unsigned short int type, unsigned int expires, void *buffer, unsigned short int *buf_len) {
	
	struct node *node;
	/*
	 * -- Entering cache critical section
	 */
	pthread_rwlock_rdlock (&cache->rwlock);
	
	node = btsearch(cache->tree, host, type);

	if (node == NULL) {
		pthread_rwlock_unlock (&cache->rwlock);
		return 0;
	} 
	
	if (expires > node->payload.expires) {
		debug ("Item expired %d, now %d\n", node->payload.expires, expires);
		pthread_rwlock_unlock (&cache->rwlock);
		return 0;
	}
	
	memcpy (buffer, &node->payload.buffer, node->payload.buf_len);
	(*buf_len) = node->payload.buf_len;
	
	/*
	 * Exiting critical section
	 */
	pthread_rwlock_unlock (&cache->rwlock);
	
	return 1;
		
}


void cache_insert (struct cache *cache, char *host, unsigned short int type, unsigned int expires, void *buffer, unsigned short int buf_len) {
	
	/*
	 * Entering a critical section to add the results of the query
	 */
	pthread_rwlock_wrlock(&cache->rwlock);
	btinsert (&cache->tree, host, type, expires, buffer, buf_len);
	pthread_rwlock_unlock(&cache->rwlock);
	
}

void cache_prune (struct cache *cache, unsigned int timestamp) {
	
	pthread_rwlock_wrlock(&cache->rwlock);
	btprune (&cache->tree, timestamp);
	pthread_rwlock_unlock(&cache->rwlock);
	
}

void cache_tidyup (struct cache *cache, unsigned int timestamp) {
	
	struct node *orig_tree;
	
	pthread_rwlock_wrlock(&cache->rwlock);
	
	btprune (&cache->tree, timestamp);
	orig_tree = cache->tree;
	cache->tree = btbalance(cache->tree);
	btdestroy(orig_tree);
	
	pthread_rwlock_unlock(&cache->rwlock);
	
}

void cache_print (struct cache *cache) {
	
	pthread_rwlock_rdlock(&cache->rwlock);
	btprint(cache->tree);
	printf ("Cached domains count: %d\n", btcount(cache->tree));
	printf ("Binary tree maximum depth: %d\n", btdepth(cache->tree));
	pthread_rwlock_unlock(&cache->rwlock);
	
}

unsigned int cache_count (struct cache *cache) {
	
	unsigned int count;
	
	pthread_rwlock_rdlock(&cache->rwlock);
	count = btcount(cache->tree);
	pthread_rwlock_unlock(&cache->rwlock);
	
	return count;
	
}

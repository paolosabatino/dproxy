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
	pthread_mutex_init (&cache->mutex, NULL);
	
	return cache;
	
}

void cache_destroy (struct cache *cache) {

	if (cache == NULL);
		return;
		
	pthread_mutex_lock(&cache->mutex);
	
	btdestroy (cache->tree);
	cache->tree = NULL;
	
	pthread_mutex_unlock(&cache->mutex);
	
	pthread_mutex_destroy(&cache->mutex);
	
	free (cache);
	
}


int cache_search (struct cache *cache, char *host, unsigned short int type, unsigned int expires, void *buffer, unsigned short int *buf_len) {
	
	struct node *node;
	/*
	 * -- Entering cache critical section
	 */
	pthread_mutex_lock (&cache->mutex);
	
	node = btsearch(cache->tree, host, type);

	if (node == NULL) {
		pthread_mutex_unlock (&cache->mutex);
		return 0;
	} 
	
	if (expires > node->payload.expires) {
		debug ("Item expired %d, now %d\n", node->payload.expires, expires);
		pthread_mutex_unlock (&cache->mutex);
		return 0;
	}
	
	memcpy (buffer, &node->payload.buffer, node->payload.buf_len);
	(*buf_len) = node->payload.buf_len;
	
	/*
	 * Exiting critical section
	 */
	pthread_mutex_unlock (&cache->mutex);
	
	return 1;
		
}


void cache_insert (struct cache *cache, char *host, unsigned short int type, unsigned int expires, void *buffer, unsigned short int buf_len) {
	
	/*
	 * Entering a critical section to add the results of the query
	 */
	pthread_mutex_lock(&cache->mutex);
	btinsert (&cache->tree, host, type, expires, buffer, buf_len);
	pthread_mutex_unlock(&cache->mutex);
	
}

void cache_prune (struct cache *cache, unsigned int timestamp) {
	
	pthread_mutex_lock(&cache->mutex);
	btprune (&cache->tree, timestamp);
	pthread_mutex_unlock(&cache->mutex);
	
}

void cache_tidyup (struct cache *cache, unsigned int timestamp) {
	
	struct node *orig_tree;
	
	pthread_mutex_lock(&cache->mutex);
	
	btprune (&cache->tree, timestamp);
	orig_tree = cache->tree;
	cache->tree = btbalance(cache->tree);
	btdestroy(orig_tree);
	
	pthread_mutex_unlock(&cache->mutex);
	
}

void cache_print (struct cache *cache) {
	
	pthread_mutex_lock(&cache->mutex);
	btprint(cache->tree);
	printf ("Cached domains count: %d\n", btcount(cache->tree));
	printf ("Binary tree maximum depth: %d\n", btdepth(cache->tree));
	pthread_mutex_unlock(&cache->mutex);
	
}

unsigned int cache_count (struct cache *cache) {
	
	unsigned int count;
	
	pthread_mutex_lock(&cache->mutex);
	count = btcount(cache->tree);
	pthread_mutex_unlock(&cache->mutex);
	
	return count;
	
}

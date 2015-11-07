#include <pthread.h>
#include "btree.h"

struct cache {
	struct node *tree;
	pthread_rwlock_t rwlock;
};

struct cache *cache_new ();
void cache_destroy (struct cache *cache);
int cache_search (struct cache *, char *, unsigned short int, unsigned int, void *, unsigned short int *);
void cache_insert (struct cache *, char *, unsigned short int , unsigned int , void *, unsigned short int);
void cache_print (struct cache *);
unsigned int cache_count (struct cache *) ;
void cache_prune (struct cache *, unsigned int);
void cache_tidyup (struct cache *, unsigned int);

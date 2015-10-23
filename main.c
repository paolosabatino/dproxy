#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include "btree.h"

#define TEST_KEYS 1000000

int main (int argc, char *argv[]) {

    struct node *tree = NULL;
    struct node *balanced_copy;
    struct node *res;
    int key;

    for (key=0; key < TEST_KEYS; key++) {
        btinsert(&tree, rand() % TEST_KEYS, key);
    }

    //btprint(tree);

    int count = btcount(tree);
    struct timespec st_time;
    struct timespec en_time;
    float time_diff_ms;

    printf ("Tree generated with %d nodes\n", count);

    printf ("Performing benchmark\n");

    clock_gettime(CLOCK_REALTIME, &st_time);
    for (key=0; key < TEST_KEYS; key++) {
        res = btsearch(tree, key);
    }
    clock_gettime(CLOCK_REALTIME, &en_time);

    time_diff_ms = ((en_time.tv_sec - st_time.tv_sec) * 1000) + ((en_time.tv_nsec - st_time.tv_nsec) / (float)1000000);

    printf ("Time: %f ms - Average per node: %f ms\n", time_diff_ms, (time_diff_ms / count));

    printf ("Performing decimation\n");

    for (key=0; key < TEST_KEYS; key++) {
        if (rand() % 5 == 0) {
            btdelete(&tree, key);
        }
    }

    printf ("New node count: %d\n", btcount(tree));

    printf ("Benchmarking again\n");

    clock_gettime(CLOCK_REALTIME, &st_time);
    for (key=0; key < TEST_KEYS; key++) {
        res = btsearch(tree, key);
    }
    clock_gettime(CLOCK_REALTIME, &en_time);

    time_diff_ms = ((en_time.tv_sec - st_time.tv_sec) * 1000) + ((en_time.tv_nsec - st_time.tv_nsec) / (float)1000000);

    printf ("Time: %f ms - Average per node: %f ms\n", time_diff_ms, (time_diff_ms / count));

    printf ("Creating a new balanced tree from the existing\n");
    balanced_copy = btbalance(tree);
    printf ("Copy done, doing benchmarks on balanced tree. Nodes: %d\n", btcount(balanced_copy));

    clock_gettime(CLOCK_REALTIME, &st_time);
    for (key=0; key < TEST_KEYS; key++) {
        res = btsearch(balanced_copy, key);
    }
    clock_gettime(CLOCK_REALTIME, &en_time);

    time_diff_ms = ((en_time.tv_sec - st_time.tv_sec) * 1000) + ((en_time.tv_nsec - st_time.tv_nsec) / (float)1000000);

    printf ("Time: %f ms - Average per node: %f ms\n", time_diff_ms, (time_diff_ms / count));

    btdestroy(tree);
    btdestroy(balanced_copy);

    printf ("Tree purged\n");

    return 0;

}
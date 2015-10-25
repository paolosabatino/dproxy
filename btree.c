#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include "btree.h"
#include "dns.h"

int inline _compare(struct node *node, char *host, unsigned short int type) {
	
	int case_compare;
	
	case_compare = strncmp(host, node->payload.host, DNS_NAME_SIZE);

	if (case_compare > 0)
		return 1;
	else if (case_compare < 0)
		return -1;

	if (type > node->payload.type)
		return 1;
	else if (type < node->payload.type)
		return -1;

	return 0;
	
}

void btinsert(struct node **tree, char *host, unsigned short int type, unsigned int expires, void *buffer, unsigned short int buf_len) {

	int compare;
	
    while (*tree != NULL) {
		
		compare = _compare((*tree), host, type);

        if (compare > 0)
            tree = &(*tree)->right;
        else if (compare < 0)
            tree = &(*tree)->left;
        else {
			memcpy (&(*tree)->payload.buffer, buffer, buf_len);
			(*tree)->payload.buf_len = buf_len;
			(*tree)->payload.expires = expires;
            return;
        }

    }

    (*tree) = (struct node *)malloc(sizeof(struct node));
    strncpy ((*tree)->payload.host, host, DNS_NAME_SIZE);
    (*tree)->payload.type = type;
	memcpy (&(*tree)->payload.buffer, buffer, buf_len);
	(*tree)->payload.buf_len = buf_len;
	(*tree)->payload.expires = expires;
    (*tree)->left = NULL;
    (*tree)->right = NULL;
    return;

}

struct node *btsearch(struct node *tree, char *host, unsigned int type) {
	
	int compare;
	
    while (tree != NULL) {
		
		compare = _compare(tree, host, type);
		
        if (compare == 0)
            return tree;

        if (compare > 0)
            tree = tree->right;
        else
            tree = tree->left;

    }

    return NULL;

}

void btdestroy(struct node *tree) {

    if (tree == NULL)
        return;

    btdestroy (tree->left);
    btdestroy (tree->right);
    free (tree);

}

void _remove_node (struct node **parent, struct node *node) {

    struct node *minimum;
    struct node *minimum_parent;

    if (node->left == NULL && node->right == NULL) {
        /*
            Simple case, the node has no children. Detach it from the parent and free memory
        */
        (*parent) = NULL;
        free (node);

    } else if (node->left == NULL) {

        /*
            Node with only a right child. Attach the child to the parent and free the node
        */
        (*parent) = node->right;
        free (node);

    } else if (node->right == NULL) {
        /*
            Node with only a left child. Attach the child to the parent and free the node
         */
        (*parent) = node->left;
        free (node);

    } else {

        /*
            Complex case: node with two children.
        */

        /*
            Find the smallest value in the immediate right subtree of the target node
        */
        minimum_parent = node;
        minimum = node->right;
        while (minimum->left != NULL) {
            minimum_parent = minimum;
            minimum = minimum->left;
        }

        /*
            Substitute the payload of the target node with the immediate larger node
        */
        memcpy (&node->payload, &minimum->payload, sizeof(struct payload));

        /*
            Remove the minimum node, that has no left child
        */
        if (minimum_parent->left == minimum)
            minimum_parent->left = minimum->right;
        else
            minimum_parent->right = minimum->right;

        free (minimum);

    }

}

void btdelete(struct node **tree, char *host, unsigned short int type) {

    struct node **parent = tree;
    struct node *node = (*tree);
    
    int compare;
    
    while (node != NULL) {
		
		compare = _compare(node, host, type);

        if (compare > 0) {

            parent = &node->right;
            node = node->right;

        } else if (compare < 0) {

            parent = &node->left;
            node = node->left;

        } else if (compare == 0) {

            _remove_node (parent, node);
            return;

        }

    }

}

void btprint (struct node *tree) {

    if (tree == NULL)
        return;

    btprint (tree->left);
    printf ("Domain %s with expiration time: %d\n", tree->payload.host, tree->payload.expires);
    btprint (tree->right);


}

void _copy_balanced_tree (struct node *origin_tree, struct node **balanced) {

    if (origin_tree == NULL)
        return;

    btinsert(balanced, origin_tree->payload.host, origin_tree->payload.type, origin_tree->payload.expires, (void *)&origin_tree->payload.buffer, origin_tree->payload.buf_len);
    _copy_balanced_tree(origin_tree->left, balanced);
    _copy_balanced_tree(origin_tree->right, balanced);

}

/**
 * Creates a balanced *copy* of a binary tree recreating all the nodes
 */
struct node *btbalance (struct node *origin_tree) {

    struct node *balanced = NULL;

    _copy_balanced_tree(origin_tree, &balanced);

    return balanced;

}

/**
 * Removes all the nodes that have the expires field below given timestamp
 */
void btprune (struct node **tree, unsigned int timestamp) {
	
	if ((*tree) == NULL)
		return;
		
	btprune (&(*tree)->left, timestamp);
	btprune (&(*tree)->right, timestamp);
	
	if (timestamp > (*tree)->payload.expires) {
		_remove_node(tree, (*tree));
		return;
	}
	
}

/**
 * Calculated the number of node of the binary tree structure
 */
int btcount (struct node *tree) {

    if (tree == NULL)
        return 0;

    return btcount(tree->left) + btcount(tree->right) + 1;

}


/**
 * Calculates the maximum depth of the binary tree structure
 */
int btdepth (struct node *tree) {
	
	int left_depth;
	int right_depth;
	
	if (tree == NULL)
		return 0;
		
	left_depth = btdepth(tree->left);
	right_depth = btdepth(tree->right);
	
	if (left_depth > right_depth)
		return left_depth + 1;
	else
		return right_depth + 1;
	
}

#include "btree.h"

int main (int argc, char *argv[]) {
	
	struct node *tree = NULL;
	struct node *lookup;
	struct in_addr ip;
	int idx;
	int random;
	char text[64];
	
	printf ("Binary tree test\n");
	
	for (idx = 0; idx < 10; idx++) {
		sprintf (text, "chiave %d", rand() % 16);
		bt_insert (&tree, text, &ip, idx);
	}
	
	printf ("Printing\n");
	print_tree (tree);
	
	for (idx = 0; idx < 16; idx++) {
		sprintf (text, "chiave %d", idx);
		lookup = bt_search (tree, text);
		if (lookup == NULL)
			printf ("%s not found\n", text);
		else
			printf ("%s has been found with idx %d\n", lookup->key, lookup->expires);
	}
	
	bt_delete_tree (tree);
	
	printf ("Done\n");
	
}

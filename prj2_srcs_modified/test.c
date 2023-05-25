#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct item {
	int j;
	struct item* link;
};
struct item* root;
struct item* curr;
int main(void)
{
	root = malloc(sizeof(struct item));
	root->j=1;
	root->link = NULL;
	curr = root->link;
	if(!curr){
		printf("NULL!\n");
	}
}
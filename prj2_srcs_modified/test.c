#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    char* cmd = "buy 1 34";
	int parsed_id, parsed_quant, i, j, k;
	int empty_space[3];
	char string_id[10], string_quant[10];
	for(i=0; cmd[i]!=' '; i++){ }
	empty_space[0] = i;
	for(j=i+1; cmd[j]!=' '; j++){ }
	empty_space[1] = j;
	strncpy(string_id, cmd + i + 1, j-i-1);
	for(k=j+1; cmd[k]; k++){ }
	empty_space[2] = k;
	strncpy(string_quant, cmd + j + 1, k-j-1);
    printf("id: %s\n", string_id);
    printf("quant: %s\n", string_quant);
    printf("i:%d j:%d k:%d\n", i, j, k);
}
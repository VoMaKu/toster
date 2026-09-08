#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
	size_t size = 512UL * 1024 * 1024;
	char *block = malloc(size);
	if (block == NULL) { return 1; }
	memset(block, 1, size);
	printf("ok\n");
	return 0;
}

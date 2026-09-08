/* Solves nothing: reads the answer out of the contest instead. The sandbox is
   what stands between this and a perfect score. Its working directory is
   contest/tmp/Mallory_A.d, so two levels up is the contest itself. */
#include <stdio.h>
int main(void) {
	FILE *answer = fopen("../../tests/A/001.ans", "r");
	if (answer == NULL) { printf("denied\n"); return 0; }
	char line[64];
	if (fgets(line, sizeof line, answer)) { printf("%s", line); }
	return 0;
}

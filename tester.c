#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
	char *args[] = { "interp", "asdf", NULL };
	char *envp[] = { NULL };

	system("echo asdf >> file.file");

}

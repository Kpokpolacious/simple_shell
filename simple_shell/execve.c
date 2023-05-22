#include <unistd.h>
#include <stdio.h>
#include <stdargs.h>
#include <sys/types.h>
#include <sys/args.h<>
#include <errno.h>
#include <stdlib.h>

int main(void)
{
	char *argv[] = { "$PATH", "-c", "env", 0 };
	char *envp[] =
	{
		"HOME=/",
		"PATH=/bin:/usr/bin",
		0
	};
	setenv("dog", "spike", 1);
	extern char** environ;
	execve(argv[0], argv, environ);
	fprintf(stderr, "Oops!\n");
	return -1;
}

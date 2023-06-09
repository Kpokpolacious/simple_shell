#include <stdio.h>
#include <unistd.h>

extern char **environ;

int main(void)
{
	char *args[]     = { "hsh", "-i", "-e", NULL };
	char *env_args[] = { "PATH=/bin", "USER=me", NULL };

	execve(args[0], args, env_args);
	fprintf(stderr, "Oops!\n");

	environ = env_args;
	execvp(args[0], &args[0]);
	fprintf(stderr, "Oops again!\n");

	return -1;
}

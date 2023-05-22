#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <netdb.h>

#define BUFSIZE		4096

static char *check = "/var/tmp/listen";

void cat(void) /*concatenates filesd to standard output*/
{
	char buf[BUFSIZE];
	int r;
	int fd = open(check, O_WRONLY);

	while (r = read(fd, buf, BUFSIZE) > 0)
	{
		write(STDOUT_FILENO, buf, r);
		close(fd);
	}
}

void handler(int sig)
{
	signal(sig, &handler);
	unlink(check);
	exit(1);
}

int main(int argc, char *argv[])
{
	int status;
	char *hostname = getenv("HOSTNAME");
	int fd;

	if (access(check, F_OK) != 0)
	{
		mkfifo(check, 0666);
		chmod(check, S_IWOTH | S_IWGRP | S_IXOTH);
	}
	else
	{
		printf("execve is already running, exiting.\n");
		exit(1);
	}
	if (access(check, F_OK) == 0)
	{
		signal(SIGINT, &handler);
	}
	pid_t pid = fork();

	if (pid < 0)
	{
		perror("fork");
		return (-1);
	}
	if (pid == 0)
	{
		execve("/usr/bin/clear", argv, NULL);
		perror("exec");
	}
	waitpid(pid, &status, 0);
	if (hostname)
		printf("Login to %s and use 'hello' to chat terminal:\n", hostname);
	while (1)
		cat();
	return (0);
}

/* FORK.C */
#include <stdio.h>

#include <unistd.h>

#include <sys/types.h>

/*
 * fork - creates a new process by duplicating the calling process
 * Return: Always 0
 */
int main(void) /* Entry point */

{

	pid_t pid;

	printf("Before fork:\n");

	printf("PID = %u, PPID = %u\n", getpid(), getppid());

	pid = fork();

	if (pid == -1)

	perror("fork() failure");

	if (pid == 0)

	{

	printf("Before sleep:\nPID = %u, PPID = %u\n", getpid(), getppid());

	sleep(10);

	printf("Child process:\n");

	printf("After sleep:\nPID = %u, PPID = %u\n", getpid(), getppid());

	}

	else

	{

	printf("Parent process exiting...\n");

	}

	return (0);

}

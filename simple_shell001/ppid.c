#include <stdio.h>

#include <unistd.h>

/*
 * int main(void) - Entry point of the program.
 *
 * This function retrieves the parent process ID (PPID) and prints it.
 * Return: Always 0
 */

int main(void)
{

	pid_t my_pid;

	my_pid = getppid();

	printf("%u\n", my_pid);

	return (0);
}

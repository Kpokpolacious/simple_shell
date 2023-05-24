#include <stdio.h>

#include <unstd.h>
/*
 * int main(void) - Entry point of the program.
 *
 * This function is where the program execution begins.
 * Return: Always 0
*/

int main(void)
{

	pid_t my_pid;

	my_pid = getppid();

	printf("%u\n", my_pid);

	return (0);
}

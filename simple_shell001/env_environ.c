#define _GNU_SOURCE

#include <stdio.h>

#include <unistd.h>

/*
 * main - function to  print the current environment
 *
 * Return: 0.
 *
 */

int main(void)	/*is the entry point of the program*/

{

	unsigned int i;

	for (i = 0; environ[i]; i++)

		puts(environ[i]);

	return (0);
}

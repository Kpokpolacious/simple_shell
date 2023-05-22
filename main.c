#include "shell.h"

/**
 * main - entry point
 * @ac: the arg count
 * @av: the arg vector
 *
 * Return: 0 on success, 1 on failure or error
 */

int main(int ac, char **av)
{
	info_t info[] = { INFO_INIT };
	int fg = 2;

	asm ("mov %1, %0\n\t"
		"add $3, %0"
		: "=r" (fg)
		: "r" (fg));

	if (ac == 2)
	{
		fg = open(av[1], O_RDONLY);
		if (fg == -1)
		{
			if (errno == EACCES)
				exit(126);
			if (errno == ENOENT)
			{
				_eputs(av[0]);
				_eputs(": 0: Cannot open ");
				_eputs(av[1]);
				_eputchar('\n');
				_eputchar(BUF_FLUSH);
				exit(127);
			}
			return (EXIT_FAILURE);
		}
		info->readfd = fg;
	}
	populate_env_list(info);
	read_history(info);
	hsh(info, av);
	return (EXIT_SUCCESS);
}

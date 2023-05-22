#include "shell.h"

/**
 * _erratoi - converts string to integer
 * @s: string to be converted
 * Return: 0 if no numbers found in string, converted number otherwise
 * -1 on error
 */

int _erratoi(char *s)
{
	int a = 0;
	unsigned long int result = 0;

	if (*s == '+')
		s++;
	for (a = 0;  s[a] != '\0'; a++)
	{
		if (s[a] >= '0' && s[a] <= '9')
		{
			result *= 10;
			result += (s[a] - '0');
			if (result > INT_MAX)
				return (-1);
		}
		else
			return (-1);
	}
	return (result);
}

/**
 * print_error - prints error message
 * @info: parameter and return info struct
 * @estr: string containing specified error type
 * Return: 0 if no numbers in string, otherwise converted number
 * -1 on error
 */

void print_error(info_t *info, char *estr)
{
	_eputs(info->fname);
	_eputs(": ");
	print_d(info->line_count, STDERR_FILENO);
	_eputs(": ");
	_eputs(info->argv[0]);
	_eputs(": ");
	_eputs(estr);
}

/**
 * print_d - function prints a decimal integer (base 10)
 * @input: input
 * @fd: filedescriptor to write
 *
 * Return: number of printed characters
 */

int print_d(int input, int fd)
{
	int (*__putchar)(char) = _putchar;
	int a, count = 0;
	unsigned int _abs_, current;

	if (fd == STDERR_FILENO)
		__putchar = _eputchar;
	if (input < 0)
	{
		_abs_ = -input;
		__putchar('-');
		count++;
	}
	else
		_abs_ = input;
	current = _abs_;
	for (a = 1000000000; a > 1; a /= 10)
	{
		if (_abs_ / a)
		{
			__putchar('0' + current / a);
			count++;
		}
		current %= a;
	}
	__putchar('0' + current);
	count++;

	return (count);
}

/**
 * convert_number - converter function
 * @num: the number
 * @base: the base
 * @flags: arg flags
 *
 * Return: string
 */

char *convert_number(long int num, int base, int flags)
{
	static char *array;
	static char buffer[50];
	char sign = 0;
	char *ptr;
	unsigned long f = num;

	if (!(flags & CONVERT_UNSIGNED) && num < 0)
	{
		f = -num;
		sign = '-';

	}
	array = flags & CONVERT_LOWERCASE ? "0123456789abcdef" : "0123456789ABCDEF";
	ptr = &buffer[49];
	*ptr = '\0';

	do	{
		*--ptr = array[f % base];
		f /= base;
	} while (f != 0);

	if (sign)
		*--ptr = sign;
	return (ptr);
}

/**
 * remove_comments - replaces the first instance of '#' with '\0'
 * @buf: the address of string to modify
 *
 * Return: 0
 */

void remove_comments(char *buf)
{
	int a;

	for (a = 0; buf[a] != '\0'; a++)
		if (buf[a] == '#' && (!a || buf[a - 1] == ' '))
		{
			buf[a] = '\0';
			break;
		}
}

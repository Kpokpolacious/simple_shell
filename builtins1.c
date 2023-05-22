#include "shell.h"

/**
 * _myhistory - displays history list, one command per line
 * @info: Structure containing potential arguments
 *  Return: Always 0
 */

int _myhistory(info_t *info)
{
	print_list(info->history);
	return (0);
}

/**
 * unset_alias - sets an alias to a string
 * @info: the parameter struct
 * @str: string alias
 *
 * Return: 0 on success, 1 on error
 */

int unset_alias(info_t *info, char *str)
{
	char *p, q;
	int r;

	p = _strchr(str, '=');
	if (!p)
		return (1);
	q = *p;
	*p = 0;
	r = delete_node_at_index(&(info->alias),
		get_node_index(info->alias, node_starts_with(info->alias, str, -1)));
	*p = q;
	return (r);
}

/**
 * set_alias - sets alias to a string
 * @info: the parameter struct
 * @str: string alias
 *
 * Return: 0 on success, 1 on error
 */

int set_alias(info_t *info, char *str)
{
	char *p;

	p = _strchr(str, '=');
	if (!p)
		return (1);
	if (!*++p)
		return (unset_alias(info, str));

	unset_alias(info, str);
	return (add_node_end(&(info->alias), str, 0) == NULL);
}

/**
 * print_alias - prints alias string
 * @node: alias node
 *
 * Return: 0 on success, 1 on error
 */

int print_alias(list_t *node)
{
	char *p = NULL, *q = NULL;

	if (node)
	{
		p = _strchr(node->str, '=');
		for (q = node->str; q <= p; q++)
			_putchar(*q);
		_putchar('\'');
		_puts(p + 1);
		_puts("'\n");
		return (0);
	}
	return (1);
}

/**
 * _myalias - mimics alias builtin
 * @info: Structure containing potential arguments
 *  Return: Always 0
 */

int _myalias(info_t *info)
{
	int l = 0;
	char *p = NULL;
	list_t *node = NULL;

	if (info->argc == 1)
	{
		node = info->alias;
		while (node)
		{
			print_alias(node);
			node = node->next;
		}
		return (0);
	}
	for (l = 1; info->argv[l]; l++)
	{
		p = _strchr(info->argv[l], '=');
		if (p)
			set_alias(info, info->argv[l]);
		else
			print_alias(node_starts_with(info->alias, info->argv[l], '='));
	}

	return (0);
}

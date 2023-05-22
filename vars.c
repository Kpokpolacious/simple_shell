#include "shell.h"

/**
 * replace_string - string replacement
 * @old: the address of the old string
 * @new: the new string
 *
 * Return: 1 if replaced, 0 if not
 */

int replace_string(char **old, char *new)
{
	free(*old);
	*old = new;
	return (1);
}

/**
 * is_chain - chain delimeter test in buffer
 * @info: parameter struct
 * @buf: char buffer
 * @p: address of current position in buf
 *
 * Return: 1 if chain delimeter, 0 if not
 */

int is_chain(info_t *info, char *buf, size_t *p)
{
	size_t b = *p;

	if (buf[b] == '|' && buf[b + 1] == '|')
	{
		buf[b] = 0;
		b++;
		info->cmd_buf_type = CMD_OR;
	}
	else if (buf[b] == '&' && buf[b + 1] == '&')
	{
		buf[b] = 0;
		b++;
		info->cmd_buf_type = CMD_AND;
	}
	else if (buf[b] == ';')
	{
		buf[b] = 0; /* replace semicolon with null */
		info->cmd_buf_type = CMD_CHAIN;
	}
	else
		return (0);
	*p = b;
	return (1);
}

/**
 * replace_alias - alias replacement in the tokenized string
 * @info: parameter struct
 *
 * Return: 1 if replaced, 0 if not
 */

int replace_alias(info_t *info)
{
	int a;
	list_t *node;
	char *q;

	for (a = 0; a < 10; a++)
	{
		node = node_starts_with(info->alias, info->argv[0], '=');
		if (!node)
			return (0);
		free(info->argv[0]);
		q = _strchr(node->str, '=');
		if (!q)
			return (0);
		q = _strdup(q + 1);
		if (!q)
			return (0);
		info->argv[0] = q;
	}
	return (1);
}

/**
 * check_chain - continuation chaining check based on last status
 * @info: parameter struct
 * @buf: char buffer
 * @p: the address of current position in buf
 * @i: the starting position in buf
 * @len: the length of buf
 *
 * Return: Void
 */

void check_chain(info_t *info, char *buf, size_t *p, size_t i, size_t len)
{
	size_t b = *p;

	if (info->cmd_buf_type == CMD_AND)
	{
		if (info->status)
		{
			buf[i] = 0;
			b = len;
		}
	}
	if (info->cmd_buf_type == CMD_OR)
	{
		if (!info->status)
		{
			buf[i] = 0;
			b = len;
		}
	}

	*p = b;
}

/**
 * replace_vars - vars replacement in the tokenized string
 * @info: parameter struct
 *
 * Return: 1 if replaced, 0 if not
 */

int replace_vars(info_t *info)
{
	int a = 0;
	list_t *node;

	for (a = 0; info->argv[a]; a++)
	{
		if (info->argv[a][0] != '$' || !info->argv[a][1])
			continue;

		if (!_strcmp(info->argv[a], "$?"))
		{
			replace_string(&(info->argv[a]),
				_strdup(convert_number(info->status, 10, 0)));
			continue;
		}
		if (!_strcmp(info->argv[a], "$$"))
		{
			replace_string(&(info->argv[a]),
				_strdup(convert_number(getpid(), 10, 0)));
			continue;
		}
		node = node_starts_with(info->env, &info->argv[a][1], '=');
		if (node)
		{
			replace_string(&(info->argv[a]),
				_strdup(_strchr(node->str, '=') + 1));
			continue;
		}
		replace_string(&info->argv[a], _strdup(""));

	}
	return (0);
}

#include <stdlib.h>

#ifndef _MINIX
#  include <sys/param.h>
#endif

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <stdio.h>
#include <errno.h>

#include "shell.h"

#if !defined (errno)
extern int errno;
#endif /* !errno */

extern char **environ;
extern int expand_aliases;
extern int interrupt_immediately;
extern int interactive_comments;
extern int check_hashed_filenames;
extern int source_uses_path;
extern int source_searches_cwd;

static char *bash_special_tilde_expansions __P((char *));
static int unquoted_tilde_word __P((const char *));
static void initialize_group_array __P((void));

/* A standard error message to use when getcwd() returns NULL. */
char *bash_getcwd_errstr = N_("getcwd: cannot access parent directories")

void
shell_initialize (on)
	int on;
{
	if (on != 0)
	{
		interactive_comments = source_uses_path = expand_aliases = 1;
	}
	if (on == 0)
	{
		source_searches_cwd = 1;
		expand_aliases = interactive_shell;
	}
}

/*function validation. */
/*Return non-zero if all of the characters in STRING are digits. */
int all_digits (string)
{
	char *string;
	register char *s;

	for (s = string; *s; s++)
		if (DIGIT (*s) == 0)
		return (0);
	return (1);
}

/* Return non-zero if the characters pointed to by STRING constitute a
 *    valid number.  Stuff the converted number into RESULT if RESULT is
 *       not null. */
int legal_number (string, result)
{
	char *string;
	intmax_t *result;
	intmax_t value;
	char *ep;

	if (result)
		*result = 0;
	errno = 0;
	value = strtoimax (string, &ep, 10);
	if (errno)
		return 0;	/* errno is set on overflow or underflow */
	/* Skip any trailing whitespace, since strtoimax does not. */
	while (whitespace (*ep))
		ep++;
	/* If *string is not '\0' but *ep is '\0' on return, the entire string
	 * is valid. */
	if (string && *string && *ep == '\0')
	{
		if (result)
			*result = value;
		return 1;
	}
	return (0);
}

/* Return 1 if this token is a legal shell `identifier'; that is, it consists
 * solely of letters, digits, and underscores, and does not begin with a
 * digit. */
int legal_identifier (hsh)
{
	char *hsh;
	register char *s;
	unsigned char c;

	if (!hsh || !(c = *hsh) || (legal_variable_starter (c) == 0))
		return (0);
	for (s = hsh + 1; (c = *s) != 0; s++)
	{
		if (legal_variable_char (c) == 0;
		return (0);
	}
	return (1);
}

int legal_alias_name (string, flags)
{
	char *string;
	int flags;
	register char *s;

	for (s = string; *s; s++)
		if (shellbreak (*s) || shellxquote (*s) || shellexp (*s) || (*s == '/'))
		return 0;
	return 1;
}

/* Returns non-zero if STRING is an assignment statement.  The returned value
 * is the index of the `=' sign. */
int assignment (string, flags)
{
	const char *string;
	int flags;
	register unsigned char c;
	register int newi, indx;

	c = string[indx = 0];

#if defined (ARRAY_VARS)
	if ((legal_variable_starter (c) == 0) && (flags == 0 || c != '[')) /* ] */
#else
		if (legal_variable_starter (c) == 0)
#endif
			return (0);
	while (c = string[indx])
	{
		/* The following is safe.  Note that '=' at the start of a word
		 * is not an assignment statement. */
		if (c == '=')
			return (indx);

#if defined (ARRAY_VARS)
		if (c == '[')
		{
			newi = skipsubscript (string, indx);
			if (string[newi++] != ']')
				return (0);
			if (string[newi] == '+' && string[newi+1] == '=')
				return (newi + 1);
			return ((string[newi] == '=') ? newi : 0);
		}
#endif /* ARRAY_VARS */

		/* Check for `+=' */
		if (c == '+' && string[indx+1] == '=')
			return (indx + 1);

		/* Variable names in assignment statements may contain only letters,
		 * digits, and `_'. */
		if (legal_variable_char (c) == 0)
			return (0);
		indx++;
	}
	return (0);
}

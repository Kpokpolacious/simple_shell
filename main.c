nclude "main.h"
#include "config.h"

#include "bashtypes.h"
#include "posixstat.h"
#include "posixtime.h"

#if defined (qnx)
#  if defined (qnx6)
#    include <sy/netmgr.h>
#  else
#    include <sys/vc.h>
#  endif /* !qnx6 */
#endif /* qnx */

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <stdio.h>
#include "chartypes.h"
#include <pwd.h>
#include "bashansi.h"
#include "bashintl.h"

#include "shell.h"
#include "flags.h"
#include "execute_cmd.h"
#include "findcmd.h"
#include "mailcheck.h"
#include "input.h"
#include "hashcmd.h"
#include "pathexp.h"

#include "builtins/getopt.h"
#include "builtins/common.h"

#if defined (READLINE)
#  include "bashline.h"
#  include <readline/readline.h>
#else
#  include <tilde/tilde.h>
#endif

#if defined (HISTORY)
#  include "bashhist.h"
#  include <readline/history.h>
#endif /* HISTORY */

#if defined (PROGRAMMABLE_COMPLETION)
#  include "pcomplete.h"
#endif

#define TEMPENV_HASH_BUCKETS	4	/* must be power of two */

#define ifsname(s)	((s)[0] == 'I' && (s)[1] == 'F' && (s)[2] == 'S' && (s)[3] == '\0')

extern char **environ;

/* Variables used here and defined in other files. */
extern int posixly_correct;
extern int line_number;
extern int subshell_environment, indirection_level, subshell_level;
extern int build_version, patch_level;
extern int expanding_redir;
extern char *dist_version, *release_status;
extern char *shell_name;
extern char *primary_prompt, *secondary_prompt;
extern char *current_host_name;
extern sh_builtin_func_t *this_shell_builtin;
extern SHELL_VAR *this_shell_function;
extern char *the_printed_command_except_trap;
extern char *this_command_name;
extern char *command_execution_string;
extern time_t shell_start_time;

#if defined (READLINE)
extern int no_line_editing;
extern int perform_hostname_completion;
#endif

/* The list of shell variables that the user has created at the global
 *    scope, or that came from the environment. */
VAR_CONTEXT *global_variables = (VAR_CONTEXT *)NULL;

/* The current list of shell variables, including function scopes */
VAR_CONTEXT *shell_variables = (VAR_CONTEXT *)NULL;

/* The list of shell functions that the user has created, or that came from
 *    the environment. */
HASH_TABLE *shell_functions = (HASH_TABLE *)NULL;

#if defined (DEBUGGER)
/* The table of shell function definitions that the user defined or that
 *    came from the environment. */
HASH_TABLE *shell_function_defs = (HASH_TABLE *)NULL;
#endif

/* The current variable context.  This is really a count of how deep into
 *    executing functions we are. */
int variable_context = 0;

/* The set of shell assignments which are made only in the environment
 *    for a single command. */
HASH_TABLE *temporary_env = (HASH_TABLE *)NULL;

/* Set to non-zero if an assignment error occurs while putting variables
 *    into the temporary environment. */
int tempenv_assign_error;

/* Some funky variables which are known about specially.  Here is where
 *    "$*", "$1", and all the cruft is kept. */
char *dollar_vars[10];
WORD_LIST *rest_of_args = (WORD_LIST *)NULL;

/* The value of $$. */
pid_t dollar_dollar_pid;

/* An array which is passed to commands as their environment.  It is
 *    manufactured from the union of the initial environment and the
 *       shell variables that are marked for export. */
char **export_env = (char **)NULL;
static int export_env_index;
static int export_env_size;

#if defined (READLINE)
static int winsize_assignment;		/* currently assigning to LINES or COLUMNS */
static int winsize_assigned;		/* assigned to LINES or COLUMNS */
#endif

/* Non-zero means that we have to remake EXPORT_ENV. */
int array_needs_making = 1;

/* The number of times BASH has been executed.  This is set
 *    by initialize_variables (). */
int shell_level = 0;

/* Some forward declarations. */
static void set_machine_vars __P((void));
static void set_home_var __P((void));
static void set_shell_var __P((void));
static char *get_bash_name __P((void));
static void initialize_shell_level __P((void));
static void uidset __P((void));
#if defined (ARRAY_VARS)
static void make_vers_array __P((void));
#endif

static SHELL_VAR *null_assign __P((SHELL_VAR *, char *, arrayind_t));
#if defined (ARRAY_VARS)
static SHELL_VAR *null_array_assign __P((SHELL_VAR *, char *, arrayind_t));
#endif
static SHELL_VAR *get_self __P((SHELL_VAR *));

#if defined (ARRAY_VARS)
static SHELL_VAR *init_dynamic_array_var __P((char *, sh_var_value_func_t *, sh_var_assign_func_t *, int));
#endif

static SHELL_VAR *assign_seconds __P((SHELL_VAR *, char *, arrayind_t));
static SHELL_VAR *get_seconds __P((SHELL_VAR *));
static SHELL_VAR *init_seconds_var __P((void));

static int brand __P((void));
static void sbrand __P((unsigned long));		/* set bash random number generator. */
static SHELL_VAR *assign_random __P((SHELL_VAR *, char *, arrayind_t));
static SHELL_VAR *get_random __P((SHELL_VAR *));

static SHELL_VAR *assign_lineno __P((SHELL_VAR *, char *, arrayind_t));
static SHELL_VAR *get_lineno __P((SHELL_VAR *));

static SHELL_VAR *assign_subshell __P((SHELL_VAR *, char *, arrayind_t));
static SHELL_VAR *get_subshell __P((SHELL_VAR *));

#if defined (HISTORY)
static SHELL_VAR *get_histcmd __P((SHELL_VAR *));
#endif

#if defined (PUSHD_AND_POPD) && defined (ARRAY_VARS)
static SHELL_VAR *assign_dirstack __P((SHELL_VAR *, char *, arrayind_t));
static SHELL_VAR *get_dirstack __P((SHELL_VAR *));
#endif

#if defined (ARRAY_VARS)
static SHELL_VAR *get_groupset __P((SHELL_VAR *));
#endif

static SHELL_VAR *get_funcname __P((SHELL_VAR *));
static SHELL_VAR *init_funcname_var __P((void));

static void initialize_dynamic_variables __P((void));

static SHELL_VAR *hash_lookup __P((const char *, HASH_TABLE *));
static SHELL_VAR *new_shell_variable __P((const char *));
static SHELL_VAR *make_new_variable __P((const char *, HASH_TABLE *));
static SHELL_VAR *bind_variable_internal __P((const char *, char *, HASH_TABLE *, int, int));

static void free_variable_hash_data __P((PTR_T));

static VARLIST *vlist_alloc __P((int));
static VARLIST *vlist_realloc __P((VARLIST *, int));
static void vlist_add __P((VARLIST *, SHELL_VAR *, int));

static void flatten __P((HASH_TABLE *, sh_var_map_func_t *, VARLIST *, int));

static int qsort_var_comp __P((SHELL_VAR **, SHELL_VAR **));

static SHELL_VAR **vapply __P((sh_var_map_func_t *));
static SHELL_VAR **fapply __P((sh_var_map_func_t *));

static int visible_var __P((SHELL_VAR *));
static int visible_and_exported __P((SHELL_VAR *));
static int local_and_exported __P((SHELL_VAR *));
static int variable_in_context __P((SHELL_VAR *));
#if defined (ARRAY_VARS)
static int visible_array_vars __P((SHELL_VAR *));
#endif

static SHELL_VAR *bind_tempenv_variable __P((const char *, char *));
static void push_temp_var __P((PTR_T));
static void propagate_temp_var __P((PTR_T));
static void dispose_temporary_env __P((sh_free_func_t *));     

static inline char *mk_env_string __P((const char *, const char *));
static char **make_env_array_from_var_list __P((SHELL_VAR **));
static char **make_var_export_array __P((VAR_CONTEXT *));
static char **make_func_export_array __P((void));
static void add_temp_array_to_env __P((char **, int, int));

static int n_shell_variables __P((void));
static int set_context __P((SHELL_VAR *));

static void push_func_var __P((PTR_T));
static void push_exported_var __P((PTR_T));

static inline int find_special_var __P((const char *));
	       
/* Initialize the shell variables from the current environment.
 *    If PRIVMODE is nonzero, don't import functions from ENV or
 *       parse $SHELLOPTS. */
void
initialize_shell_variables (env, privmode)
	     char **env;
	          int privmode;
{
	  char *name, *string, *temp_string;
	    int c, char_index, string_index, string_length;
	      SHELL_VAR *temp_var;

	        if (shell_variables == 0)
			    {
				          shell_variables = global_variables = new_var_context ((char *)NULL, 0);
					        shell_variables->scope = 0;
						      shell_variables->table = hash_create (0);
						          }

		  if (shell_functions == 0)
			      shell_functions = hash_create (0);

#if defined (DEBUGGER)
		    if (shell_function_defs == 0)
			        shell_function_defs = hash_create (0);
#endif

		      for (string_index = 0; string = env[string_index++]; )
			          {
					        char_index = 0;
						      name = string;
						            while ((c = *string++) && c != '=')
								    	;
							          if (string[-1] == '=')
									  	char_index = string - name - 1;

								        /* If there are weird things in the environment, like `=xxx' or a
									 * 	 string without an `=', just skip them. */
								        if (char_index == 0)
											continue;

									      /* ASSERT(name[char_index] == '=') */
									      name[char_index] = '\0';
									            /* Now, name = env variable name, string = env variable value, and
										     * 	 char_index == strlen (name) */

									            /* If exported function, define it now.  Don't import functions from
										     * 	 the environment in privileged mode. */
									            if (privmode == 0 && read_but_dont_execute == 0 && STREQN ("() {", string, 4))
											    	{
														  string_length = strlen (string);
														  	  temp_string = (char *)xmalloc (3 + string_length + char_index);

															  	  strcpy (temp_string, name);
																  	  temp_string[char_index] = ' ';
																	  	  strcpy (temp_string + char_index + 1, string);

																		  	  parse_and_execute (temp_string, name, SEVAL_NONINT|SEVAL_NOHIST);

																			  	  /* Ancient backwards compatibility.  Old versions of bash exported
																				   * 	     functions like name()=() {...} */
																			  	  if (name[char_index - 1] == ')' && name[char_index - 2] == '(')
																					  	    name[char_index - 2] = '\0';

																				  	  if (temp_var = find_function (name))
																						  	    {
																								    	      VSETATTR (temp_var, (att_exported|att_imported));
																									      	      array_needs_making = 1;
																										      	    }
																					  	  else
																							  	    report_error (_("error importing function definition for `%s'"), name);

																						  	  /* ( */
																						  	  if (name[char_index - 1] == ')' && name[char_index - 2] == '\0')
																								  	    name[char_index - 2] = '(';		/* ) */
																							  	}
#if defined (ARRAY_VARS)
#  if 0
										          /* Array variables may not yet be exported. */
										          else if (*string == '(' && string[1] == '[' && string[strlen (string) - 1] == ')')
												  	{
															  string_length = 1;
															  	  temp_string = extract_array_assignment_list (string, &string_length);
																  	  temp_var = assign_array_from_string (name, temp_string);
																	  	  FREE (temp_string);
																		  	  VSETATTR (temp_var, (att_exported | att_imported));
																			  	  array_needs_making = 1;
																				  	}
#  endif
#endif
											        else
														{
																  temp_var = bind_variable (name, string, 0);
																  	  VSETATTR (temp_var, (att_exported | att_imported));
																	  	  array_needs_making = 1;
																		  	}

												      name[char_index] = '=';
												            /* temp_var can be NULL if it was an exported function with a syntax
													     * 	 error (a different bug, but it still shouldn't dump core). */
												            if (temp_var && function_p (temp_var) == 0)	/* XXX not yet */
														    	{
																	  CACHE_IMPORTSTR (temp_var, name);
																	  	}
													        }

		        set_pwd ();

			  /* Set up initial value of $_ */
#if 0
			  temp_var = bind_variable ("_", dollar_vars[0], 0);
#else
			    temp_var = set_if_not ("_", dollar_vars[0]);
#endif

			      /* Remember this pid. */
			      dollar_dollar_pid = getpid ();

			        /* Now make our own defaults in case the vars that we think are
				 *      important are missing. */
			        temp_var = set_if_not ("PATH", DEFAULT_PATH_VALUE);
#if 0
				  set_auto_export (temp_var);	/* XXX */
#endif

				    temp_var = set_if_not ("TERM", "dumb");
#if 0
				      set_auto_export (temp_var);	/* XXX */
#endif

#if defined (qnx)
				        /* set node id -- don't import it from the environment */
				        {
						    char node_name[22];
#  if defined (qnx6)
						        netmgr_ndtostr(ND2S_LOCAL_STR, ND_LOCAL_NODE, node_name, sizeof(node_name));
#  else
							    qnx_nidtostr (getnid (), node_name, sizeof (node_name));
#  endif
							        temp_var = bind_variable ("NODE", node_name, 0);
								    set_auto_export (temp_var);
								      }
#endif

					  /* set up the prompts. */
					  if (interactive_shell)
						      {
#if defined (PROMPT_STRING_DECODE)
							            set_if_not ("PS1", primary_prompt);
#else
								          if (current_user.uid == -1)
										  	get_current_user_info ();
									        set_if_not ("PS1", current_user.euid == 0 ? "# " : primary_prompt);
#endif
										      set_if_not ("PS2", secondary_prompt);
										          }
					    set_if_not ("PS4", "+ ");

					      /* Don't allow IFS to be imported from the environment. */
					      temp_var = bind_variable ("IFS", " \t\n", 0);
					        setifs (temp_var);

						  /* Magic machine types.  Pretty convenient. */
						  set_machine_vars ();

						    /* Default MAILCHECK for interactive shells.  Defer the creation of a
						     *      default MAILPATH until the startup files are read, because MAIL
						     *           names a mail file if MAILPATH is not set, and we should provide a
						     *                default only if neither is set. */
						    if (interactive_shell)
							        {
									      temp_var = set_if_not ("MAILCHECK", posixly_correct ? "600" : "60");
									            VSETATTR (temp_var, att_integer);
										        }

						      /* Do some things with shell level. */
						      initialize_shell_level ();

						        set_ppid ();

							  /* Initialize the `getopts' stuff. */
							  temp_var = bind_variable ("OPTIND", "1", 0);
							    VSETATTR (temp_var, att_integer);
							      getopts_reset (0);
							        bind_variable ("OPTERR", "1", 0);
								  sh_opterr = 1;

								    if (login_shell == 1)
									        set_home_var ();

								      /* Get the full pathname to THIS shell, and set the BASH variable
								       *      to it. */
								      name = get_bash_name ();
								        temp_var = bind_variable ("BASH", name, 0);
									  free (name);

									    /* Make the exported environment variable SHELL be the user's login
									     *      shell.  Note that the `tset' command looks at this variable
									     *           to determine what style of commands to output; if it ends in "csh",
									     *                then C-shell commands are output, else Bourne shell commands. */
									    set_shell_var ();

									      /* Make a variable called BASH_VERSION which contains the version info. */
									      bind_variable ("BASH_VERSION", shell_version_string (), 0);
#if defined (ARRAY_VARS)
									        make_vers_array ();
#endif

										  if (command_execution_string)
											      bind_variable ("BASH_EXECUTION_STRING", command_execution_string, 0);

										    /* Find out if we're supposed to be in Posix.2 mode via an
										     *      environment variable. */
										    temp_var = find_variable ("POSIXLY_CORRECT");
										      if (!temp_var)
											          temp_var = find_variable ("POSIX_PEDANTIC");
										        if (temp_var && imported_p (temp_var))
												    sv_strict_posix (temp_var->name);

#if defined (HISTORY)
											  /* Set history variables to defaults, and then do whatever we would
											   *      do if the variable had just been set.  Do this only in the case
											   *           that we are remembering commands on the history list. */
											  if (remember_on_history)
												      {
													            name = bash_tilde_expand (posixly_correct ? "~/.sh_history" : "~/.bash_history", 0);

														          set_if_not ("HISTFILE", name);
															        free (name);

																      set_if_not ("HISTSIZE", "500");
																            sv_histsize ("HISTSIZE");
																	        }
#endif /* HISTORY */

											    /* Seed the random number generator. */
											    sbrand (dollar_dollar_pid + shell_start_time);

											      /* Handle some "special" variables that we may have inherited from a
											       *      parent shell. */
											      if (interactive_shell)
												          {
														        temp_var = find_variable ("IGNOREEOF");
															      if (!temp_var)
																      	temp_var = find_variable ("ignoreeof");
															            if (temp_var && imported_p (temp_var))
																	    	sv_ignoreeof (temp_var->name);
																        }

#if defined (HISTORY)
											        if (interactive_shell && remember_on_history)
													    {
														          sv_history_control ("HISTCONTROL");
															        sv_histignore ("HISTIGNORE");
																    }
#endif /* HISTORY */

#if defined (READLINE) && defined (STRICT_POSIX)
												  /* POSIXLY_CORRECT will only be 1 here if the shell was compiled
												   *      -DSTRICT_POSIX */
												  if (interactive_shell && posixly_correct && no_line_editing == 0)
													      rl_prefer_env_winsize = 1;
#endif /* READLINE && STRICT_POSIX */

												       /*
													*       * 24 October 2001
													*             *
													*                   * I'm tired of the arguing and bug reports.  Bash now leaves SSH_CLIENT
													*                         * and SSH2_CLIENT alone.  I'm going to rely on the shell_level check in
													*                               * isnetconn() to avoid running the startup files more often than wanted.
													*                                     * That will, of course, only work if the user's login shell is bash, so
													*                                           * I've made that behavior conditional on SSH_SOURCE_BASHRC being defined
													*                                                 * in config-top.h.
													*                                                       */
#if 0
												    temp_var = find_variable ("SSH_CLIENT");
												      if (temp_var && imported_p (temp_var))
													          {
															        VUNSETATTR (temp_var, att_exported);
																      array_needs_making = 1;
																          }
												        temp_var = find_variable ("SSH2_CLIENT");
													  if (temp_var && imported_p (temp_var))
														      {
															            VUNSETATTR (temp_var, att_exported);
																          array_needs_making = 1;
																	      }
#endif

													    /* Get the user's real and effective user ids. */
													    uidset ();

													      /* Initialize the dynamic variables, and seed their values. */
													      initialize_dynamic_variables ();
}

/* **************************************************************** */
/*								    */
/*	     Setting values for special shell variables		    */
/*								    */
/* **************************************************************** */

static void
set_machine_vars ()
{
	  SHELL_VAR *temp_var;

	    temp_var = set_if_not ("HOSTTYPE", HOSTTYPE);
	      temp_var = set_if_not ("OSTYPE", OSTYPE);
	        temp_var = set_if_not ("MACHTYPE", MACHTYPE);

		  temp_var = set_if_not ("HOSTNAME", current_host_name);
}

/* Set $HOME to the information in the password file if we didn't get
 *    it from the environment. */

/* This function is not static so the tilde and readline libraries can
 *    use it. */
char *
sh_get_home_dir ()
{
	  if (current_user.home_dir == 0)
		      get_current_user_info ();
	    return current_user.home_dir;
}

static void
set_home_var ()
{
	  SHELL_VAR *temp_var;

	    temp_var = find_variable ("HOME");
	      if (temp_var == 0)
		          temp_var = bind_variable ("HOME", sh_get_home_dir (), 0);
#if 0
	        VSETATTR (temp_var, att_exported);
#endif
}

/* Set $SHELL to the user's login shell if it is not already set.  Call
 *    get_current_user_info if we haven't already fetched the shell. */
static void
set_shell_var ()
{
	  SHELL_VAR *temp_var;

	    temp_var = find_variable ("SHELL");
	      if (temp_var == 0)
		          {
				        if (current_user.shell == 0)
							get_current_user_info ();
					      temp_var = bind_variable ("SHELL", current_user.shell, 0);
					          }
#if 0
	        VSETATTR (temp_var, att_exported);
#endif
}

static char *
get_bash_name ()
{
	  char *name;

	    if ((login_shell == 1) && RELPATH(shell_name))
		        {
				      if (current_user.shell == 0)
					      	get_current_user_info ();
				            name = savestring (current_user.shell);
					        }
	      else if (ABSPATH(shell_name))
		          name = savestring (shell_name);
	        else if (shell_name[0] == '.' && shell_name[1] == '/')
			    {
				          /* Fast path for common case. */
				          char *cdir;
					        int len;

						      cdir = get_string_value ("PWD");
						            if (cdir)
								    	{
											  len = strlen (cdir);
											  	  name = (char *)xmalloc (len + strlen (shell_name) + 1);
												  	  strcpy (name, cdir);
													  	  strcpy (name + len, shell_name + 1);
														  	}
							          else
									  	name = savestring (shell_name);
								      }
		  else
			      {
				            char *tname;
					          int s;

						        tname = find_user_command (shell_name);

							      if (tname == 0)
								      	{
											  /* Try the current directory.  If there is not an executable
											   * 	     there, just punt and use the login shell. */
											  s = file_status (shell_name);
											  	  if (s & FS_EXECABLE)
													  	    {
															    	      tname = make_absolute (shell_name, get_string_value ("PWD"));
																      	      if (*shell_name == '.')
																		      		{
																							  name = sh_canonpath (tname, PATH_CHECKDOTDOT|PATH_CHECKEXISTS);
																							  		  if (name == 0)
																										  		    name = tname;
																									  		  else
																												  		    free (tname);
																											  		}
																	      	     else
																			     		name = tname;
																		     	    }
												  	  else
														  	    {
																    	      if (current_user.shell == 0)
																		      		get_current_user_info ();
																	      	      name = savestring (current_user.shell);
																		      	    }
													  	}
							            else
									    	{
												  name = full_pathname (tname);
												  	  free (tname);
													  	}
								        }

		    return (name);
}

void
adjust_shell_level (change)
	     int change;
{
	  char new_level[5], *old_SHLVL;
	    intmax_t old_level;
	      SHELL_VAR *temp_var;

	        old_SHLVL = get_string_value ("SHLVL");
		  if (old_SHLVL == 0 || *old_SHLVL == '\0' || legal_number (old_SHLVL, &old_level) == 0)
			      old_level = 0;

		    shell_level = old_level + change;
		      if (shell_level < 0)
			          shell_level = 0;
		        else if (shell_level > 1000)
				    {
					          internal_warning (_("shell level (%d) too high, resetting to 1"), shell_level);
						        shell_level = 1;
							    }

			  /* We don't need the full generality of itos here. */
			  if (shell_level < 10)
				      {
					            new_level[0] = shell_level + '0';
						          new_level[1] = '\0';
							      }
			    else if (shell_level < 100)
				        {
						      new_level[0] = (shell_level / 10) + '0';
						            new_level[1] = (shell_level % 10) + '0';
							          new_level[2] = '\0';
								      }
			      else if (shell_level < 1000)
				          {
						        new_level[0] = (shell_level / 100) + '0';
							      old_level = shell_level % 100;
							            new_level[1] = (old_level / 10) + '0';
								          new_level[2] = (old_level % 10) + '0';
									        new_level[3] = '\0';
										    }

			        temp_var = bind_variable ("SHLVL", new_level, 0);
				  set_auto_export (temp_var);
}

static void
initialize_shell_level ()
{
	  adjust_shell_level (1);
}

/* If we got PWD from the environment, update our idea of the current
 *    working directory.  In any case, make sure that PWD exists before
 *       checking it.  It is possible for getcwd () to fail on shell startup,
 *          and in that case, PWD would be undefined.  If this is an interactive
 *             login shell, see if $HOME is the current working directory, and if
 *                that's not the same string as $PWD, set PWD=$HOME. */

void
set_pwd ()
{
	  SHELL_VAR *temp_var, *home_var;
	    char *temp_string, *home_string;

	      home_var = find_variable ("HOME");
	        home_string = home_var ? value_cell (home_var) : (char *)NULL;

		  temp_var = find_variable ("PWD");
		    if (temp_var && imported_p (temp_var) &&
				          (temp_string = value_cell (temp_var)) &&
					        same_file (temp_string, ".", (struct stat *)NULL, (struct stat *)NULL))
			        set_working_directory (temp_string);
		      else if (home_string && interactive_shell && login_shell &&
				      	   same_file (home_string, ".", (struct stat *)NULL, (struct stat *)NULL))
			          {
					        set_working_directory (home_string);
						      temp_var = bind_variable ("PWD", home_string, 0);
						            set_auto_export (temp_var);
							        }
		        else
				    {
					          temp_string = get_working_directory ("shell-init");
						        if (temp_string)
									{
											  temp_var = bind_variable ("PWD", temp_string, 0);
											  	  set_auto_export (temp_var);
												  	  free (temp_string);
													  	}
							    }

			  /* According to the Single Unix Specification, v2, $OLDPWD is an
			   *      `environment variable' and therefore should be auto-exported.
			   *           Make a dummy invisible variable for OLDPWD, and mark it as exported. */
			  temp_var = bind_variable ("OLDPWD", (char *)NULL, 0);
			    VSETATTR (temp_var, (att_exported | att_invisible));
}

/* Make a variable $PPID, which holds the pid of the shell's parent.  */
void
set_ppid ()
{
	  char namebuf[INT_STRLEN_BOUND(pid_t) + 1], *name;
	    SHELL_VAR *temp_var;

	      name = inttostr (getppid (), namebuf, sizeof(namebuf));
	        temp_var = find_variable ("PPID");
		  if (temp_var)
			      VUNSETATTR (temp_var, (att_readonly | att_exported));
		    temp_var = bind_variable ("PPID", name, 0);
		      VSETATTR (temp_var, (att_readonly | att_integer));
}

static void
uidset ()
{
	  char buff[INT_STRLEN_BOUND(uid_t) + 1], *b;
	    register SHELL_VAR *v;

	      b = inttostr (current_user.uid, buff, sizeof (buff));
	        v = find_variable ("UID");
		  if (v == 0)
			      {
				            v = bind_variable ("UID", b, 0);
					          VSETATTR (v, (att_readonly | att_integer));
						      }

		    if (current_user.euid != current_user.uid)
			        b = inttostr (current_user.euid, buff, sizeof (buff));

		      v = find_variable ("EUID");
		        if (v == 0)
				    {
					          v = bind_variable ("EUID", b, 0);
						        VSETATTR (v, (att_readonly | att_integer));
							    }
}

#if defined (ARRAY_VARS)
static void
make_vers_array ()
{
	  SHELL_VAR *vv;
	    ARRAY *av;
	      char *s, d[32], b[INT_STRLEN_BOUND(int) + 1];

	        unbind_variable ("BASH_VERSINFO");

		  vv = make_new_array_variable ("BASH_VERSINFO");
		    av = array_cell (vv);
		      strcpy (d, dist_version);
		        s = xstrchr (d, '.');
			  if (s)
				      *s++ = '\0';
			    array_insert (av, 0, d);
			      array_insert (av, 1, s);
			        s = inttostr (patch_level, b, sizeof (b));
				  array_insert (av, 2, s);
				    s = inttostr (build_version, b, sizeof (b));
				      array_insert (av, 3, s);
				        array_insert (av, 4, release_status);
					  array_insert (av, 5, MACHTYPE);

					    VSETATTR (vv, att_readonly);
}
#endif /* ARRAY_VARS */

/* Set the environment variables $LINES and $COLUMNS in response to
 *    a window size change. */
void
sh_set_lines_and_columns (lines, cols)
	     int lines, cols;
{
	  char val[INT_STRLEN_BOUND(int) + 1], *v;

	    /* If we are currently assigning to LINES or COLUMNS, don't do anything. */
	    if (winsize_assignment)
		        return;

	      v = inttostr (lines, val, sizeof (val));
	        bind_variable ("LINES", v, 0);

		  v = inttostr (cols, val, sizeof (val));
		    bind_variable ("COLUMNS", v, 0);
}

/* **************************************************************** */
/*								    */
/*		   Printing variables and values		    */
/*								    */
/* **************************************************************** */

/* Print LIST (a list of shell variables) to stdout in such a way that
 *    they can be read back in. */
void
print_var_list (list)
	     register SHELL_VAR **list;
{
	  register int i;
	    register SHELL_VAR *var;

	      for (i = 0; list && (var = list[i]); i++)
		          if (invisible_p (var) == 0)
				        print_assignment (var);
}

/* Print LIST (a list of shell functions) to stdout in such a way that
 *    they can be read back in. */
void
print_func_list (list)
	     register SHELL_VAR **list;
{
	  register int i;
	    register SHELL_VAR *var;

	      for (i = 0; list && (var = list[i]); i++)
		          {
				        printf ("%s ", var->name);
					      print_var_function (var);
					            printf ("\n");
						        }
}
      
/* Print the value of a single SHELL_VAR.  No newline is
 *    output, but the variable is printed in such a way that
 *       it can be read back in. */
void
print_assignment (var)
	     SHELL_VAR *var;
{
	  if (var_isset (var) == 0)
		      return;

	    if (function_p (var))
		        {
				      printf ("%s", var->name);
				            print_var_function (var);
					          printf ("\n");
						      }
#if defined (ARRAY_VARS)
	      else if (array_p (var))
		          print_array_assignment (var, 0);
#endif /* ARRAY_VARS */
	        else
			    {
				          printf ("%s=", var->name);
					        print_var_value (var, 1);
						      printf ("\n");
						          }
}

/* Print the value cell of VAR, a shell variable.  Do not print
 *    the name, nor leading/trailing newline.  If QUOTE is non-zero,
 *       and the value contains shell metacharacters, quote the value
 *          in such a way that it can be read back in. */
void
print_var_value (var, quote)
	     SHELL_VAR *var;
	          int quote;
{
	  char *t;

	    if (var_isset (var) == 0)
		        return;

	      if (quote && posixly_correct == 0 && ansic_shouldquote (value_cell (var)))
		          {
				        t = ansic_quote (value_cell (var), 0, (int *)0);
					      printf ("%s", t);
					            free (t);
						        }
	        else if (quote && sh_contains_shell_metas (value_cell (var)))
			    {
				          t = sh_single_quote (value_cell (var));
					        printf ("%s", t);
						      free (t);
						          }
		  else
			      printf ("%s", value_cell (var));
}

/* Print the function cell of VAR, a shell variable.  Do not
 *    print the name, nor leading/trailing newline. */
void
print_var_function (var)
	     SHELL_VAR *var;
{
	  if (function_p (var) && var_isset (var))
		      printf ("%s", named_function_string ((char *)NULL, function_cell(var), 1));
}

/* **************************************************************** */
/*								    */
/*		 	Dynamic Variables			    */
/*								    */
/* **************************************************************** */

/* DYNAMIC VARIABLES
 *
 *    These are variables whose values are generated anew each time they are
 *       referenced.  These are implemented using a pair of function pointers
 *          in the struct variable: assign_func, which is called from bind_variable
 *             and, if arrays are compiled into the shell, some of the functions in
 *                arrayfunc.c, and dynamic_value, which is called from find_variable.
 *
 *                   assign_func is called from bind_variable_internal, if
 *                      bind_variable_internal discovers that the variable being assigned to
 *                         has such a function.  The function is called as
 *                         	SHELL_VAR *temp = (*(entry->assign_func)) (entry, value, ind)
 *                         	   and the (SHELL_VAR *)temp is returned as the value of bind_variable.  It
 *                         	      is usually ENTRY (self).  IND is an index for an array variable, and
 *                         	         unused otherwise.
 *
 *                         	            dynamic_value is called from find_variable_internal to return a `new'
 *                         	               value for the specified dynamic varible.  If this function is NULL,
 *                         	                  the variable is treated as a `normal' shell variable.  If it is not,
 *                         	                     however, then this function is called like this:
 *                         	                     	tempvar = (*(var->dynamic_value)) (var);
 *
 *                         	                     	   Sometimes `tempvar' will replace the value of `var'.  Other times, the
 *                         	                     	      shell will simply use the string value.  Pretty object-oriented, huh?
 *
 *                         	                     	         Be warned, though: if you `unset' a special variable, it loses its
 *                         	                     	            special meaning, even if you subsequently set it.
 *
 *                         	                     	               The special assignment code would probably have been better put in
 *                         	                     	                  subst.c: do_assignment_internal, in the same style as
 *                         	                     	                     stupidly_hack_special_variables, but I wanted the changes as
 *                         	                     	                        localized as possible.  */

#define INIT_DYNAMIC_VAR(var, val, gfunc, afunc) \
	  do \
	      { \
		            v = bind_variable (var, (val), 0); \
		            v->dynamic_value = gfunc; \
		            v->assign_func = afunc; \
		          } \
			    while (0)

#define INIT_DYNAMIC_ARRAY_VAR(var, gfunc, afunc) \
	  do \
	      { \
		            v = make_new_array_variable (var); \
		            v->dynamic_value = gfunc; \
		            v->assign_func = afunc; \
		          } \
			    while (0)

static SHELL_VAR *
null_assign (self, value, unused)
	     SHELL_VAR *self;
	          char *value;
		       arrayind_t unused;
{
	  return (self);
}

#if defined (ARRAY_VARS)
static SHELL_VAR *
null_array_assign (self, value, ind)
	     SHELL_VAR *self;
	          char *value;
		       arrayind_t ind;
{
	  return (self);
}
#endif

/* Degenerate `dynamic_value' function; just returns what's passed without
 *    manipulation. */
static SHELL_VAR *
get_self (self)
	     SHELL_VAR *self;
{
	  return (self);
}

#if defined (ARRAY_VARS)
/* A generic dynamic array variable initializer.  Intialize array variable
 *    NAME with dynamic value function GETFUNC and assignment function SETFUNC. */
static SHELL_VAR *
init_dynamic_array_var (name, getfunc, setfunc, attrs)
	     char *name;
	          sh_var_value_func_t *getfunc;
		       sh_var_assign_func_t *setfunc;
		            int attrs;
{
	  SHELL_VAR *v;

	    v = find_variable (name);
	      if (v)
		          return (v);
	        INIT_DYNAMIC_ARRAY_VAR (name, getfunc, setfunc);
		  if (attrs)
			      VSETATTR (v, attrs);
		    return v;
}
#endif


/* The value of $SECONDS.  This is the number of seconds since shell
 *    invocation, or, the number of seconds since the last assignment + the
 *       value of the last assignment. */
static intmax_t seconds_value_assigned;

static SHELL_VAR *
assign_seconds (self, value, unused)
	     SHELL_VAR *self;
	          char *value;
		       arrayind_t unused;
{
	  if (legal_number (value, &seconds_value_assigned) == 0)
		      seconds_value_assigned = 0;
	    shell_start_time = NOW;
	      return (self);
}

static SHELL_VAR *
get_seconds (var)
	     SHELL_VAR *var;
{
	  time_t time_since_start;
	    char *p;

	      time_since_start = NOW - shell_start_time;
	        p = itos(seconds_value_assigned + time_since_start);

		  FREE (value_cell (var));

		    VSETATTR (var, att_integer);
		      var_setvalue (var, p);
		        return (var);
}

static SHELL_VAR *
init_seconds_var ()
{
	  SHELL_VAR *v;

	    v = find_variable ("SECONDS");
	      if (v)
		          {
				        if (legal_number (value_cell(v), &seconds_value_assigned) == 0)
							seconds_value_assigned = 0;
					    }
	        INIT_DYNAMIC_VAR ("SECONDS", (v ? value_cell (v) : (char *)NULL), get_seconds, assign_seconds);
		  return v;      
}
     
/* The random number seed.  You can change this by setting RANDOM. */
static unsigned long rseed = 1;
static int last_random_value;
static int seeded_subshell = 0;

/* A linear congruential random number generator based on the example
 *    one in the ANSI C standard.  This one isn't very good, but a more
 *       complicated one is overkill. */

/* Returns a pseudo-random number between 0 and 32767. */
static int
brand ()
{
	  rseed = rseed * 1103515245 + 12345;
	    return ((unsigned int)((rseed >> 16) & 32767));	/* was % 32768 */
}

/* Set the random number generator seed to SEED. */
static void
sbrand (seed)
	     unsigned long seed;
{
	  rseed = seed;
	    last_random_value = 0;
}

static SHELL_VAR *
assign_random (self, value, unused)
	     SHELL_VAR *self;
	          char *value;
		       arrayind_t unused;
{
	  sbrand (strtoul (value, (char **)NULL, 10));
	    if (subshell_environment)
		        seeded_subshell = 1;
	      return (self);
}

int
get_random_number ()
{
	  int rv;

	    /* Reset for command and process substitution. */
	    if (subshell_environment && seeded_subshell == 0)
		        {
				      sbrand (rseed + getpid() + NOW);
				            seeded_subshell = 1;
					        }

	      do
		          rv = brand ();
	        while (rv == last_random_value);
	        return rv;
}

static SHELL_VAR *
get_random (var)
	     SHELL_VAR *var;
{
	  int rv;
	    char *p;

	      rv = get_random_number ();
	        last_random_value = rv;
		  p = itos (rv);

		    FREE (value_cell (var));

		      VSETATTR (var, att_integer);
		        var_setvalue (var, p);
			  return (var);
}

static SHELL_VAR *
assign_lineno (var, value, unused)
	     SHELL_VAR *var;
	          char *value;
		       arrayind_t unused;
{
	  intmax_t new_value;

	    if (value == 0 || *value == '\0' || legal_number (value, &new_value) == 0)
		        new_value = 0;
	      line_number = new_value;
	        return var;
}

/* Function which returns the current line number. */
static SHELL_VAR *
get_lineno (var)
	     SHELL_VAR *var;
{
	  char *p;
	    int ln;

	      ln = executing_line_number ();
	        p = itos (ln);
		  FREE (value_cell (var));
		    var_setvalue (var, p);
		      return (var);
}

static SHELL_VAR *
assign_subshell (var, value, unused)
	     SHELL_VAR *var;
	          char *value;
		       arrayind_t unused;
{
	  intmax_t new_value;

	    if (value == 0 || *value == '\0' || legal_number (value, &new_value) == 0)
		        new_value = 0;
	      subshell_level = new_value;
	        return var;
}

static SHELL_VAR *
get_subshell (var)
	     SHELL_VAR *var;
{
	  char *p;

	    p = itos (subshell_level);
	      FREE (value_cell (var));
	        var_setvalue (var, p);
		  return (var);
}

static SHELL_VAR *
get_bash_command (var)
	     SHELL_VAR *var;
{
	  char *p;

	    
	    if (the_printed_command_except_trap)
		        p = savestring (the_printed_command_except_trap);
	      else
		          {
				        p = (char *)xmalloc (1);
					      p[0] = '\0';
					          }
	        FREE (value_cell (var));
		  var_setvalue (var, p);
		    return (var);
}

#if defined (HISTORY)
static SHELL_VAR *
get_histcmd (var)
	     SHELL_VAR *var;
{
	  char *p;

	    p = itos (history_number ());
	      FREE (value_cell (var));
	        var_setvalue (var, p);
		  return (var);
}
#endif

#if defined (READLINE)
/* When this function returns, VAR->value points to malloced memory. */
static SHELL_VAR *
get_comp_wordbreaks (var)
	     SHELL_VAR *var;
{
	  char *p;

	    /* If we don't have anything yet, assign a default value. */
	    if (rl_completer_word_break_characters == 0 && bash_readline_initialized == 0)
		        enable_hostname_completion (perform_hostname_completion);

#if 0
	      FREE (value_cell (var));
	        p = savestring (rl_completer_word_break_characters);
		  
		  var_setvalue (var, p);
#else
		    var_setvalue (var, rl_completer_word_break_characters);
#endif

		      return (var);
}

/* When this function returns, rl_completer_word_break_characters points to
 *    malloced memory. */
static SHELL_VAR *
assign_comp_wordbreaks (self, value, unused)
	     SHELL_VAR *self;
	          char *value;
		       arrayind_t unused;
{
	  if (rl_completer_word_break_characters &&
			        rl_completer_word_break_characters != rl_basic_word_break_characters)
		      free (rl_completer_word_break_characters);

	    rl_completer_word_break_characters = savestring (value);
	      return self;
}
#endif /* READLINE */

#if defined (PUSHD_AND_POPD) && defined (ARRAY_VARS)
static SHELL_VAR *
assign_dirstack (self, value, ind)
	     SHELL_VAR *self;
	          char *value;
		       arrayind_t ind;
{
	  set_dirstack_element (ind, 1, value);
	    return self;
}

static SHELL_VAR *
get_dirstack (self)
	     SHELL_VAR *self;
{
	  ARRAY *a;
	    WORD_LIST *l;

	      l = get_directory_stack ();
	        a = array_from_word_list (l);
		  array_dispose (array_cell (self));
		    dispose_words (l);
		      var_setarray (self, a);
		        return self;
}
#endif /* PUSHD AND POPD && ARRAY_VARS */

#if defined (ARRAY_VARS)
/* We don't want to initialize the group set with a call to getgroups()
 *    unless we're asked to, but we only want to do it once. */
static SHELL_VAR *
get_groupset (self)
	     SHELL_VAR *self;
{
	  register int i;
	    int ng;
	      ARRAY *a;
	        static char **group_set = (char **)NULL;

		  if (group_set == 0)
			      {
				            group_set = get_group_list (&ng);
					          a = array_cell (self);
						        for (i = 0; i < ng; i++)
									array_insert (a, i, group_set[i]);
							    }
		    return (self);
}
#endif /* ARRAY_VARS */

/* If ARRAY_VARS is not defined, this just returns the name of any
 *    currently-executing function.  If we have arrays, it's a call stack. */
static SHELL_VAR *
get_funcname (self)
	     SHELL_VAR *self;
{
#if ! defined (ARRAY_VARS)
	  char *t;
	    if (variable_context && this_shell_function)
		        {
				      FREE (value_cell (self));
				            t = savestring (this_shell_function->name);
					          var_setvalue (self, t);
						      }
#endif
	      return (self);
}

void
make_funcname_visible (on_or_off)
	     int on_or_off;
{
	  SHELL_VAR *v;

	    v = find_variable ("FUNCNAME");
	      if (v == 0 || v->dynamic_value == 0)
		          return;

	        if (on_or_off)
			    VUNSETATTR (v, att_invisible);
		  else
			      VSETATTR (v, att_invisible);
}

static SHELL_VAR *
init_funcname_var ()
{
	  SHELL_VAR *v;

	    v = find_variable ("FUNCNAME");
	      if (v)
		          return v;
#if defined (ARRAY_VARS)
	        INIT_DYNAMIC_ARRAY_VAR ("FUNCNAME", get_funcname, null_array_assign);
#else
		  INIT_DYNAMIC_VAR ("FUNCNAME", (char *)NULL, get_funcname, null_assign);
#endif
		    VSETATTR (v, att_invisible|att_noassign);
		      return v;
}

static void
initialize_dynamic_variables ()
{
	  SHELL_VAR *v;

	    v = init_seconds_var ();

	      INIT_DYNAMIC_VAR ("BASH_COMMAND", (char *)NULL, get_bash_command, (sh_var_assign_func_t *)NULL);
	        INIT_DYNAMIC_VAR ("BASH_SUBSHELL", (char *)NULL, get_subshell, assign_subshell);

		  INIT_DYNAMIC_VAR ("RANDOM", (char *)NULL, get_random, assign_random);
		    VSETATTR (v, att_integer);
		      INIT_DYNAMIC_VAR ("LINENO", (char *)NULL, get_lineno, assign_lineno);
		        VSETATTR (v, att_integer);

#if defined (HISTORY)
			  INIT_DYNAMIC_VAR ("HISTCMD", (char *)NULL, get_histcmd, (sh_var_assign_func_t *)NULL);
			    VSETATTR (v, att_integer);
#endif

#if defined (READLINE)
			      INIT_DYNAMIC_VAR ("COMP_WORDBREAKS", (char *)NULL, get_comp_wordbreaks, assign_comp_wordbreaks);
#endif

#if defined (PUSHD_AND_POPD) && defined (ARRAY_VARS)
			        v = init_dynamic_array_var ("DIRSTACK", get_dirstack, assign_dirstack, 0);
#endif /* PUSHD_AND_POPD && ARRAY_VARS */

#if defined (ARRAY_VARS)
				  v = init_dynamic_array_var ("GROUPS", get_groupset, null_array_assign, att_noassign);

#  if defined (DEBUGGER)
				    v = init_dynamic_array_var ("BASH_ARGC", get_self, null_array_assign, att_noassign|att_nounset);
				      v = init_dynamic_array_var ("BASH_ARGV", get_self, null_array_assign, att_noassign|att_nounset);
#  endif /* DEBUGGER */
				        v = init_dynamic_array_var ("BASH_SOURCE", get_self, null_array_assign, att_noassign|att_nounset);
					  v = init_dynamic_array_var ("BASH_LINENO", get_self, null_array_assign, att_noassign|att_nounset);
#endif

					    v = init_funcname_var ();
}

/* **************************************************************** */
/*								    */
/*		Retrieving variables and values			    */
/*								    */
/* **************************************************************** */

/* How to get a pointer to the shell variable or function named NAME.
 *    HASHED_VARS is a pointer to the hash table containing the list
 *       of interest (either variables or functions). */

static SHELL_VAR *
hash_lookup (name, hashed_vars)
	     const char *name;
	          HASH_TABLE *hashed_vars;
{
	  BUCKET_CONTENTS *bucket;

	    bucket = hash_search (name, hashed_vars, 0);
	      return (bucket ? (SHELL_VAR *)bucket->data : (SHELL_VAR *)NULL);
}

SHELL_VAR *
var_lookup (name, vcontext)
	     const char *name;
	          VAR_CONTEXT *vcontext;
{
	  VAR_CONTEXT *vc;
	    SHELL_VAR *v;

	      v = (SHELL_VAR *)NULL;
	        for (vc = vcontext; vc; vc = vc->down)
			    if (v = hash_lookup (name, vc->table))
				          break;

		  return v;
}

/* Look up the variable entry named NAME.  If SEARCH_TEMPENV is non-zero,
 *    then also search the temporarily built list of exported variables.
 *       

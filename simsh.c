/*
 * Author: Fong Ching Hin Stephen
 * Description: A simple Unix shell based on BASH
 */

#define MAX_LINE_CHARS 1024
#define INTERACTIVE_PROMPT "cowrie> "
#define DEFAULT_PATH "/bin:/usr/bin"
#define WORD_SEPARATORS " \t\r\n"
#define DEFAULT_HISTORY_SHOWN 10

// These characters are always returned as single words
#define SPECIAL_CHARS "!><|"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <spawn.h>
#include <unistd.h>
#include <limits.h>
#include <glob.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "helper.h"
#include "history.h"
#include "redirection.h"
#include "color.h"

static void print_prompt();
static void execute_command(char **words, char **path, char **environment);
static void do_exit(char **words);
static char **globbing(char **tokens);
static char **glob_word(char **globbed_command, int *ntokens, char *token);
static void piping(char **tokens, char **path, char **environ);
static char *get_single_string(char **tokens);
static void construct_absolute_path(char *path, char *program, char *executable_path);
static int is_executable(char *pathname);
static int execute_executable(char **command_argv, char *path, char **environ);
static void print_and_execute_past_command(char *asciiNumber, char **path, char **environment);
static char **tokenize(char *s, char *separators, char *special_chars);
static void free_tokens(char **tokens);

int executable_exists(char **path, char *program, char *executable_path);


int main(void) {
  extern char **environ;

  // grab the 'PATH' environment variable;
  // if it isn't set, use the default path defined above
  char *pathp;
  if ((pathp = getenv("PATH")) == NULL) {
    pathp = DEFAULT_PATH;
  }
  char **path = tokenize(pathp, ":", "");

  // main loop: print prompt, read line, execute command
  while (1) {
    print_prompt();

    char line[MAX_LINE_CHARS];
    if (fgets(line, MAX_LINE_CHARS, stdin) == NULL) {
      break;
    }

    char **command_words = tokenize(line, WORD_SEPARATORS, SPECIAL_CHARS);
    execute_command(command_words, path, environ);
    free_tokens(command_words);
  }

  free_tokens(path);
  return 0;
}


int executable_exists(char **path, char *program, char *executable_path) {
  for (int i = 0; path[i] != NULL; i++) {
    construct_absolute_path(path[i], program, executable_path);
    if (is_executable(executable_path)) {
      return true;
    }
  }
  return false;
}

static void print_prompt() {
  char prompt[MAX_LINE_CHARS];

  char username[MAX_LINE_CHARS];
  getlogin_r(username, MAX_LINE_CHARS);

  char pathname[MAX_LINE_CHARS];
  getcwd(pathname, sizeof pathname);

  char hostname[MAX_LINE_CHARS];
  gethostname(hostname, MAX_LINE_CHARS);

  green();
  fprintf(stdout, "%s@%s", username, hostname);
  reset();
  fprintf(stdout, ":");

  char *homedir = getenv("HOME");
  int len = strlen(homedir);
  if (startsWith(homedir, pathname) == true) {
    snprintf(pathname, sizeof pathname, "%s%s", "~", &pathname[len]);
  }

  blue();
  fprintf(stdout, "%s", pathname);
  reset();
  fprintf(stdout, "$ ");
}


//
// Execute a command, and wait until it finishes.
//
// 'words': a NULL-terminated array of words from the input command line
// 'path': a NULL-terminated array of directories to search in;
// 'environment': a NULL-terminated array of environment variables.
//
static void execute_command(char **words, char **path, char **environment) {
  assert(words != NULL);
  assert(path != NULL);
  assert(environment != NULL);

  char *home_path = getenv("HOME");

  char **globbed_words = globbing(words);

  // name of the program
  char *program = globbed_words[0];

  if (program == NULL) {
    // nothing to do
    return;

  } else if (strcmp(program, "exit") == 0) {
    do_exit(globbed_words);

  } else if (is_pipes(globbed_words)) {
    // any command contains '|' get caught here
    piping(globbed_words, path, environment);

  } else if (is_redirection(globbed_words)) {
    if (!is_valid_redirection_position(globbed_words)) {
      write_to_history(globbed_words);
      return;

    } else if (!is_valid_redirection_program(globbed_words)) {
      write_to_history(globbed_words);
      return;
    }

    if (is_input_redirection(globbed_words) && (is_output_write_redirection(globbed_words)
        || is_output_append_redirection(globbed_words))) {
      input_output_redirection(globbed_words, path, environment);

    } else if (is_input_redirection(globbed_words)) {
      input_redirection(globbed_words, path, environment);

    } else if (is_output_write_redirection(globbed_words)) {
      output_write_redirection(globbed_words, path, environment);

    } else if (is_output_append_redirection(globbed_words)) {
      output_append_redirection(globbed_words, path, environment);

    }

  } else if (strcmp(program, "cd") == 0) {
    if (count_nwords(globbed_words) > 2) {
      print_too_many_arguments(program);
      write_to_history(globbed_words);
      return;
    }

    if (globbed_words[1] != NULL) {
     
      if (chdir(globbed_words[1]) == -1) {
        fprintf(stderr, "cd: %s: ", globbed_words[1]);
        perror("");
      }

    } else {
      chdir(home_path);
    }

  } else if (strcmp(program, "pwd") == 0) {

    char pathname[PATH_MAX];
    getcwd(pathname, sizeof pathname);
    fprintf(stdout, "%s\n", pathname);

  } else if (strcmp(program, "history") == 0) {

    if (count_nwords(globbed_words) > 2) {
      print_too_many_arguments(program);
      write_to_history(globbed_words);
      return;
    }

    // ensure if 1st arg exists it's an integer
    if (globbed_words[1] != NULL && !is_integer(globbed_words[1])) {
      fprintf(stderr, "history: %s: numeric argument required\n",
	      words[1]);
      write_to_history(words);
      return;
    }

    print_history(globbed_words[1]);
    
  } else if (strcmp(program, "!") == 0) {
    if (globbed_words[1] != NULL && !is_integer(globbed_words[1])) {
      fprintf(stderr, "!: %s: numeric argument required\n",
              words[1]);
      return;
    }

    print_and_execute_past_command(globbed_words[1], path, environment);

    return;

  } else if (strchr(program, '/') == NULL) {
    char executable_path[PATH_MAX];

    if (executable_exists(path, program, executable_path)) {
      execute_executable(globbed_words, executable_path, environment);

    } else {
      fprintf(stderr, "%s: command not found\n", program);

    }

  } else if (is_executable(program)) {
    execute_executable(globbed_words, program, environment);

  } else {
    fprintf(stderr, "%s: command not found\n", program);

  }

  write_to_history(words);
  return;
}


//
// Implement the 'exit' shell built-in, which exits the shell.
//
// Synopsis: exit [exit-status]
// Examples:
//     % exit
//     % exit 1
//
static void do_exit(char **words) {
  int exit_status = 0;

  if (words[1] != NULL) {
    if (words[2] != NULL) {
      fprintf(stderr, "exit: too many arguments\n");
    } else {
      char *endptr;
      exit_status = (int)strtol(words[1], &endptr, 10);
      if (*endptr != '\0') {
	fprintf(stderr, "exit: %s: numeric argument required\n",
		words[1]);
      }
    }
  }
  exit(exit_status);
}


// Returns an array of strings, with the last element being 'NULL'.
// 'tokens' is the output of the 'tokenize' function.
// Replace characters '*', '?', '[', or '~' appears in a word by
// all of the words matching that word.
// If there are no matches, use the word unchanged.
static char **globbing(char **tokens) {
  char **globbed_tokens = malloc(sizeof(*tokens));

  // save the program's name.
  globbed_tokens[0] = tokens[0];
  int ntokens = 1;

  if (tokens[0] == NULL) return globbed_tokens;
  // iterate through all tokens, glob each of them and append matching
  // words to the end of the array.
  for (int i = 1; tokens[i] != NULL; i++) {
    globbed_tokens = glob_word(globbed_tokens, &ntokens, tokens[i]);
  }

  globbed_tokens = realloc(globbed_tokens, sizeof(*globbed_tokens) *
                           (ntokens + 1));
  globbed_tokens[ntokens] = NULL;
  return globbed_tokens;
}


// Glob a word and append it to the end of an array of strings.
static char **glob_word(char **globbed_tokens, int *ntokens, char *token) {
  glob_t matches;
  int result = glob(token, GLOB_NOCHECK|GLOB_TILDE, NULL, &matches);

  if (result != 0) {
    // no matches, add back the original token.
    (*ntokens)++;
    globbed_tokens = realloc(globbed_tokens, sizeof(*globbed_tokens) *
                             (*ntokens));

    globbed_tokens[(*ntokens)-1] = token;
  } else {
    // has matches, append all matches to the array.
    globbed_tokens = realloc(globbed_tokens, sizeof(*globbed_tokens) *
                             ((*ntokens) + matches.gl_pathc));

    for (int i = 0; i < matches.gl_pathc; i++) {
      globbed_tokens[(*ntokens)] = matches.gl_pathv[i];
      (*ntokens)++;
    }
  }
  return globbed_tokens;
}


// handle any command contains at least one '|' in it.
static void piping(char **tokens, char **path, char **environ) {
  char *command = get_single_string(tokens);
  char **commands = tokenize(command, "|", "");

  int prev_read_pipe = -1;
  int i = 0;
  while (commands[i+1] != NULL) {

    char **components = tokenize(commands[i], WORD_SEPARATORS, SPECIAL_CHARS);

    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
      perror("pipe");
      return;
    }

    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) {
      perror("posix_spawn_file_actions_init");
      return;
    }

    // connect child process stdout to the write side of the child process pipe.
    if (posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], 1)) {
      perror("posix_spawn_file_actions_adddup2");
    }

    if (prev_read_pipe != -1) {


      // connect the stdin of the child process to the read side of the previous process's pipe
      if(posix_spawn_file_actions_adddup2(&actions, prev_read_pipe, 0) != 0)
      {
        perror("posix_spawn_file_actions_adddup2");
        return;
      }

      char executable_path[PATH_MAX];
      executable_exists(path, components[0], executable_path);

      pid_t pid;
      if (posix_spawn(&pid, executable_path, &actions, NULL, components, environ)) {
        fprintf(stderr, "%s: command not found\n", components[0]);
	return;
      }

      close(pipe_fds[1]);

    } else {
      // first component of the command
      if (strcmp(components[0], "<") == 0) {
        int nwords = count_nwords(components);
        if (nwords < 3) {
          fprintf(stderr, "Invalid pipe\n");
          return;
        }
      }

      for (int j = 0; components[j]; j++) {
        if (strcmp(components[j], "<") == 0 && j != 0) {
          fprintf(stderr, "Invalid input redirection\n");
          return;
        }
        if (strcmp(components[j], ">") == 0) {
          fprintf(stderr, "Invalid output redirection\n");
          return;
        }

      }

      if (is_input_redirection(components)) {

	int input_file = open(components[1], O_RDONLY);

        // read from the read end of the pipe instead of stdin
	if (posix_spawn_file_actions_adddup2(&actions, input_file, 0) != 0) {
	  perror("posix_spawn_file_actions_adddup2");
	  return;
	}

	char executable_path[PATH_MAX];
	executable_exists(path, components[2], executable_path); // haven't check if path exists

	pid_t pid;
	if (posix_spawn(&pid, executable_path, &actions, NULL, &components[2], environ)) {
	  fprintf(stderr, "%s: command not found\n", components[2]);
	  return;
	}

        close(pipe_fds[1]);

      } else {

	char executable_path[PATH_MAX];
	executable_exists(path, components[0], executable_path);

        posix_spawn_file_actions_addclose(&actions, pipe_fds[0]);

	pid_t pid;
	if (posix_spawn(&pid, executable_path, &actions, NULL, components, environ)) {
	  fprintf(stderr, "%s: command not found\n", components[0]);
	  return;
	}
        close(pipe_fds[1]);
      }

    }

    prev_read_pipe = pipe_fds[0];
    i++;

  }

  // last component of the command
  char **components = tokenize(commands[i], WORD_SEPARATORS, SPECIAL_CHARS);

  int pipe_fds[2];
  if (pipe(pipe_fds) == -1) {
    perror("pipe");
    return;
  }

  posix_spawn_file_actions_t actions;
  if (posix_spawn_file_actions_init(&actions) != 0) {
    perror("posix_spawn_file_actions_init");
    return;
  }

  // connect child process stdout to the write side of the child process pipe.
  if (posix_spawn_file_actions_adddup2(&actions, prev_read_pipe, 0)) {
    perror("posix_spawn_file_actions_adddup2");
  }

  if (posix_spawn_file_actions_addclose(&actions, pipe_fds[1])) {
    perror("posix_spawn_file_actions_addclose");
  }

  if (is_redirection(components)) {
    if (is_valid_redirection_position(components) &&
        is_valid_redirection_program(components)) {

      int fd;
      int nwords = count_nwords(components);

      if (is_output_write_redirection(components)) {
	fd = open(components[nwords-1], O_CREAT|O_WRONLY|O_TRUNC, 0644);

      } else if (is_output_append_redirection(components)) {
        fd = open(components[nwords-1], O_CREAT|O_WRONLY|O_APPEND, 0644);

      } else {
        fd = -1;
      }
      if (fd == -1) {
	perror(components[nwords-1]);
	return;
      }
      if (posix_spawn_file_actions_adddup2(&actions, fd, 1)) {
	perror("posix_spawn_file_actions_adddup2");
      }
      // construct the arguments array for posix_spawn
      components = get_args_for_output_redirection(components);
    } else {
      return;
    }
  }


  char executable_path[PATH_MAX];
  executable_exists(path, components[0], executable_path); // haven't check if path exists
  pid_t pid;
  if (posix_spawn(&pid, executable_path, &actions, NULL, components, environ)) {
    fprintf(stderr, "%s: command not found\n", components[0]);
    return;
  }
  close(pipe_fds[1]);

  int status;
  if (waitpid(pid, &status, 0) == -1) {
    perror("waitpid");
  }

  if (WIFEXITED(status)) {
    const int exit_status = WEXITSTATUS(status);
    fprintf(stdout, "%s exit status = %d\n", executable_path, exit_status);
  }
}


// join an array of strings into a single string, delimited by space.
static char *get_single_string(char **tokens) {
  char *command = strdup(tokens[0]);

  for (int i = 1; tokens[i] != NULL; i++) {
    // enough space for two strings and a space and NULL character.
    int size = strlen(command) + strlen(tokens[i]) + 2;
    command = realloc(command, size * sizeof(char));
    strcat(command, " ");
    strncat(command, tokens[i], strlen(tokens[i]));
  }
  return command;
}


// give a path, concatenate the program name to it and save it to
// 'executable_path'
static void construct_absolute_path(char *path, char *program, char *executable_path) {
  int path_len = strlen(path) + strlen(program) + 2;
  snprintf(executable_path, path_len, "%s/%s", path, program);
}


// given a path to the command, arguments array of the command and environ,
// executes the command.
static int execute_executable(char **command_argv, char *path, char **environ) {
  pid_t pid;
  if (posix_spawn(&pid, path, NULL, NULL, command_argv, environ)) {
    fprintf(stderr, "%s: command not found\n", command_argv[0]);
    return 1;
  }

  int status;
  if (waitpid(pid, &status, 0) == -1) {
    perror("waitpid");
    return 1;
  }

  if (WIFEXITED(status)) {
    const int exit_status = WEXITSTATUS(status);
    fprintf(stdout, "%s exit status = %d\n", path, exit_status);
  }
  
  return 0;
}


// print and execute the command in the .cowrie_history file.
static void print_and_execute_past_command(char *asciiNumber, char **path, char **environment) {
  FILE *fp = fopen(get_history_path(), "r");
  if (fp != NULL) {

    int n;
    if (asciiNumber != NULL) {
      n = atoi(asciiNumber);
    } else {
      n = get_nlines() - 1;
    }

    char buffer[2048];
    for (int i = 0; fgets(buffer, 2048, fp) != NULL; i++) {
      if (i == n) {
	printf("%s", buffer);

	char **command_words = tokenize(buffer, WORD_SEPARATORS,
					SPECIAL_CHARS);

	execute_command(command_words, path, environment);

	fclose(fp);
	return;
      }
    }
    fclose(fp);
  }
  fprintf(stderr, "!: invalid history reference\n");
}


//
// Check whether this process can execute a file.
// Use this function when searching through the directories
// in the path for an executable file
//
static int is_executable(char *pathname) {
  struct stat s;
  return
  // does the file exist?
  stat(pathname, &s) == 0 &&
  // is the file a regular file?
  S_ISREG(s.st_mode) &&
  // can we execute it?
  faccessat(AT_FDCWD, pathname, X_OK, AT_EACCESS) == 0;
}


//
// Split a string 's' into pieces by any one of a set of separators.
//
// Returns an array of strings, with the last element being 'NULL';
// The array itself, and the strings, are allocated with 'malloc(3)';
// the provided 'free_token' function can deallocate this.
//
static char **tokenize(char *s, char *separators, char *special_chars) {
  size_t n_tokens = 0;
  // malloc array guaranteed to be big enough
  char **tokens = malloc((strlen(s) + 1) * sizeof *tokens);

  while (*s != '\0') {
    // We are pointing at zero or more of any of the separators.
    // Skip leading instances of the separators.
    s += strspn(s, separators);

    // Now, 's' points at one or more characters we want to keep.
    // The number of non-separator characters is the token length.
    //
    // Trailing separators after the last token mean that, at this
    // point, we are looking at the end of the string, so:
    if (*s == '\0') {
      break;
    }

    size_t token_length = strcspn(s, separators);
    size_t token_length_without_special_chars = strcspn(s, special_chars);

    if (token_length_without_special_chars == 0) {
      token_length_without_special_chars = 1;
    }

    if (token_length_without_special_chars < token_length) {
      token_length = token_length_without_special_chars;
    }

    char *token = strndup(s, token_length);
    assert(token != NULL);
    s += token_length;

    // Add this token.
    tokens[n_tokens] = token;
    n_tokens++;
  }

  tokens[n_tokens] = NULL;
  // shrink array to correct size
  tokens = realloc(tokens, (n_tokens + 1) * sizeof *tokens);

  return tokens;
}


//
// Free an array of strings as returned by 'tokenize'.
//
static void free_tokens(char **tokens) {
  for (int i = 0; tokens[i] != NULL; i++) {
    free(tokens[i]);
  }
  free(tokens);
}

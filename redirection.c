#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <spawn.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/wait.h>

#include "simsh.h"
#include "helper.h"
#include "redirection.h"


int is_redirection(char **words) {
  // return True if 'words' contains any '<' and '>'.
  for (int i = 0; words[i] != NULL; i++) {
    if (strcmp(words[i], ">") == 0 || strcmp(words[i], "<") == 0) {
      return true;
    }
  }
  return false;
}


int is_input_redirection(char **words) {
  // return True if 'words' starts with '<'.
  return strcmp(words[0], "<") == 0;
}


int is_output_write_redirection(char **words) {
  int nwords = count_nwords(words);
  // return True if 'words' contains '>' in the second last position.
  return (nwords - 2 >= 0 && strcmp(words[nwords-2], ">") == 0) &&
         (nwords - 3 >= 0 && strcmp(words[nwords-3], ">") != 0);
}


int is_output_append_redirection(char **words) {
  int nwords = count_nwords(words);
  // return True if 'words' contains two consecutive '>' in the second
  // and third last position.
  return (nwords - 2 >= 0 && strcmp(words[nwords-2], ">") == 0 &&
          nwords - 3 >= 0 && strcmp(words[nwords-3], ">") == 0);
}


int is_valid_redirection_position(char **words) {
  int nwords = count_nwords(words);

  // if not enough words for input redirection
  if (nwords - 3 < 0 && strcmp(words[0], "<") == 0) {
    fprintf(stderr, "Invalid input redirection\n");
    return false;
  }

  // if not enough words for output append redirection
  if ((nwords - 3 >= 0 && strcmp(words[nwords-3], ">") == 0 &&
      nwords - 2 >= 0 && strcmp(words[nwords-2], ">") == 0 &&
      nwords - 4 < 0)) {
    fprintf(stderr, "Invalid output redirection\n");
    return false;
  }

  // if not enough words for output write redirection
  if ((nwords - 1 >= 0 && strcmp(words[nwords-1], ">") == 0) ||
      (nwords - 3 < 0 && strcmp(words[nwords-2], ">") == 0)) {
    fprintf(stderr, "Invalid output redirection\n");
    return false;
  }

  // check if '<' or '>' appears in invalid position.
  for (int i = 0; words[i] != NULL; i++) {
    if (strcmp(words[i], "<") == 0 && i != 0) {
      fprintf(stderr, "Invalid input redirection\n");
      return false;

    } else if (strcmp(words[i], ">") == 0 && (i != nwords - 2 &&
               i != nwords - 3)) {
      // return False if the '>' appears in position other than
      // second or third last position.
      fprintf(stderr, "Invalid output redirection\n");
      return false;
    }
  }
  // return False if the third last word is '>' but the second last
  // word is not '>'.
  if ((nwords - 3 >= 0 && strcmp(words[nwords-3], ">") == 0 &&
      nwords - 2 >= 0 && strcmp(words[nwords-2], ">") != 0)) {
    fprintf(stderr, "Invalid output redirection\n");
    return false;
  }
  return true;
}


int is_valid_redirection_program(char **words) {
  int nwords = count_nwords(words);
  // check input redirection: < file program
  if (strcmp(words[0], "<") == 0 && is_builtin_command(words[2])) {
    fprintf(stderr, "%s: I/O redirection not permitted for builtin commands\n", words[2]);
    return false;
  }

  int pos = nwords - 1;
  while (pos != 0) {
    // stop at the position of the program, since
    // command could be in the format: < file program
    if (pos - 2 >= 0 && strcmp(words[pos-2], "<") == 0) {
      break;
    }
    pos--;
  }

  // if pos = 0, then there's no '<' in 'words', also implies
  // this function is used in a larger pipe command.
  // else, pos = position of the program in 'words'.
  if (is_builtin_command(words[pos])) {
    fprintf(stderr, "%s: I/O redirection not permitted for builtin commands\n", words[pos]);
    return false;
  }
  return true;
}


void input_redirection(char **tokens, char **path, char **environ) {

  int fd = open(tokens[1], O_RDONLY);
  if (fd == -1) {
    perror(tokens[1]);
    return;
  }

  posix_spawn_file_actions_t actions;
  if (posix_spawn_file_actions_init(&actions) != 0) {
    perror("posix_spawn_file_actions_init");
    return;
  }

  // connect stdin of the program to the file.
  if (posix_spawn_file_actions_adddup2(&actions, fd, 0) != 0) {
    perror("posix_spawn_file_actions_adddup2");
    return;
  }

  char executable_path[PATH_MAX];
  if (executable_exists(path, tokens[2], executable_path)) {
    pid_t pid;
    if (posix_spawn(&pid, executable_path, &actions, NULL, &tokens[2],
                    environ) != 0) {
      fprintf(stderr, "%s: command not found\n", tokens[2]);

      return;
    }

    int status;
    if (waitpid(pid, &status, 0) == -1) {
      perror("waitpid");
      return;
    }

    if (WIFEXITED(status)) {
      const int exit_status = WEXITSTATUS(status);
      fprintf(stdout, "%s exit status = %d\n", executable_path, exit_status);
    }
  } else {
      fprintf(stderr, "%s: command not found\n", tokens[2]);
  }
}


void output_write_redirection(char **tokens, char **path, char **environ) {
  int nwords = count_nwords(tokens);

  int fd = open(tokens[nwords-1], O_CREAT|O_WRONLY|O_TRUNC, 0644);
  if (fd == -1) {
    perror(tokens[nwords-1]);
    return;
  }

  posix_spawn_file_actions_t actions;
  if (posix_spawn_file_actions_init(&actions) != 0) {
    perror("posix_spawn_file_actions_init");
    return;
  }

  // connect stdout to the file
  if (posix_spawn_file_actions_adddup2(&actions, fd, 1) != 0) {
    perror("posix_spawn_file_actions_adddup2");
    return;
  }
  char executable_path[PATH_MAX];

  if (executable_exists(path, tokens[0], executable_path)) {
    pid_t pid;
    char **args = get_args_for_output_redirection(tokens);

    if (posix_spawn(&pid, executable_path, &actions, NULL, args, environ) != 0) {
      fprintf(stderr, "%s: command not found\n", args[0]);
      return;
    }

    int status;
    if (waitpid(pid, &status, 0) == -1) {
      perror("waitpid");
      return;
    }

    if (WIFEXITED(status)) {
      const int exit_status = WEXITSTATUS(status);
      fprintf(stdout, "%s exit status = %d\n", executable_path, exit_status);
    }
  } else {
      fprintf(stderr, "%s: command not found\n", tokens[0]);
  }
}


void output_append_redirection(char **tokens, char **path, char **environ) {
  int nwords = count_nwords(tokens);
  int fd = open(tokens[nwords-1], O_CREAT|O_WRONLY|O_APPEND, 0644);
  if (fd == -1) {
    perror(tokens[nwords-1]);
    return;
  }

  posix_spawn_file_actions_t actions;
  if (posix_spawn_file_actions_init(&actions) != 0) {
    perror("posix_spawn_file_actions_init");
    return;
  }

  // connect stdout to the file
  if (posix_spawn_file_actions_adddup2(&actions, fd, 1) != 0) {
    perror("posix_spawn_file_actions_adddup2");
    return;
  }

  char executable_path[PATH_MAX];
  if (executable_exists(path, tokens[0], executable_path)) {
    pid_t pid;
    char **args = get_args_for_output_redirection(tokens);

    if (posix_spawn(&pid, executable_path, &actions, NULL, args, environ) != 0) {
      fprintf(stderr, "%s: command not found\n", args[0]);
      return;
    }

    int status;
    if (waitpid(pid, &status, 0) == -1) {
      perror("waitpid");
      return;
    }

    if (WIFEXITED(status)) {
      const int exit_status = WEXITSTATUS(status);
      fprintf(stdout, "%s exit status = %d\n", executable_path, exit_status);
    }
  } else {
      fprintf(stderr, "%s: command not found\n", tokens[0]);
  }
}


void input_output_redirection(char **tokens, char **path, char **environ) {
  
  int input_fd = open(tokens[1], O_RDONLY);
  if (input_fd == -1) {
    perror(tokens[1]);
    return;
  }

  int output_fd = -1;
  int nwords = count_nwords(tokens);
  if (is_output_write_redirection(tokens)) {
    output_fd = open(tokens[nwords-1], O_CREAT|O_WRONLY|O_TRUNC, 0644);

  } else if (is_output_append_redirection(tokens)) {
    output_fd = open(tokens[nwords-1], O_CREAT|O_WRONLY|O_APPEND, 0644);

  }

  posix_spawn_file_actions_t actions;
  if (posix_spawn_file_actions_init(&actions) != 0) {
    perror("posix_spawn_file_actions_init");
    return;
  }

  // connect stdin to the file
  if (posix_spawn_file_actions_adddup2(&actions, input_fd, 0) != 0) {
    perror("posix_spawn_file_actions_adddup2");
    return;
  }

  // connect stdout to the file
  if (posix_spawn_file_actions_adddup2(&actions, output_fd, 1) != 0) {
    perror("posix_spawn_file_actions_adddup2");
    return;
  }

  char executable_path[PATH_MAX];
  if (executable_exists(path, tokens[2], executable_path)) {
    pid_t pid;
    char **args = get_args_for_output_redirection(tokens);
    if (posix_spawn(&pid, executable_path, &actions, NULL, args, environ) != 0) {
      fprintf(stderr, "%s: command not found\n", args[0]);
      return;
    }

    int status;
    if (waitpid(pid, &status, 0) == -1) {
      perror("waitpid");
      return;
    }

    if (WIFEXITED(status)) {
      const int exit_status = WEXITSTATUS(status);
      fprintf(stdout, "%s exit status = %d\n", executable_path, exit_status);
    }
  } else {
      fprintf(stderr, "%s: command not found\n", tokens[0]);
  }
}


char **get_args_for_output_redirection(char **tokens) {
  int ntokens = count_nwords(tokens);
  char **args = malloc(sizeof(char *) * ntokens);
  int count = 0;
  // copy all words before '>'
  while (count < ntokens && strcmp(tokens[count], ">") != 0) {
    args[count] = tokens[count];
    count++;
  }
  // allocates enough space for placing NULL
  args = realloc(args, sizeof(char *) * (count + 1));
  args[count] = NULL;

  // handle the case when input redirection is next to output redirection.
  // e.g. args => < pwd cat > cd
  if (strcmp(args[0], "<") == 0) {
    args = &args[2];
  }
  return args;
}

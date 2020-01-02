#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <spawn.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "helper.h"


int is_integer(char *string) {
  for (int i = 0; string[i] != '\0'; i++) {
    // Negative number if starts with '-'
    if (i == 0 && string[i] == '-') continue;
    if (string[i] < '0' || string[i] > '9') {
      return false;
    }
  }
  return true;
}


int is_pipes(char **tokens) {
  for (int i = 0; tokens[i] != NULL; i++) {
    if (strcmp(tokens[i], "|") == 0) {
      return true;
    }
  }
  return false;
}


int count_nwords(char **words) {
  int count = 0;
  for (int i = 0; words[i] != NULL; i++) {
    count++;
  }
  return count;
}


void print_too_many_arguments(char *program) {
  fprintf(stderr, "%s: too many arguments\n", program);
}


int is_builtin_command(char *command) {
  if (strcmp(command, "cd") == 0) {
    return 1;
  } else if (strcmp(command, "pwd") == 0) {
    return 1;
  } else if (strcmp(command, "history") == 0) {
    return 1;
  } else {
    return 0;
  }
}

bool startsWith(const char *pre, const char *str) {
  size_t lenpre = strlen(pre),
	 lenstr = strlen(str);
  return lenstr < lenpre ? false : memcmp(pre, str, lenpre) == 0;
}


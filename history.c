#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "helper.h"
#include "history.h"


static int get_starting_line_number(char *asciiNumber, int nlines);


char *get_history_path() {
  char *home_path = getenv("HOME");
  assert(home_path != NULL);

  char *basename = ".cowrie_history";
  int path_len = strlen(home_path) + strlen(basename) + 2;
  char *history_path = malloc(sizeof(char) * path_len);

  snprintf(history_path, path_len, "%s/%s",
           home_path, basename);
  
  return history_path;
}


void print_history(char *asciiNumber) {
  FILE *fp = fopen(get_history_path(), "r");
  if (fp != NULL) {

    int nlines = get_nlines();
    // Get the line to start printing.
    int startingline = get_starting_line_number(asciiNumber, nlines);
    char buffer[2048];

    if (startingline != -1) {
      for (int i = 0; fgets(buffer, 2048, fp) != NULL; i++) {
	if (i >= startingline) {
	  printf("%d: %s", i, buffer);
	}
      }
    }
    fclose(fp);
  }
}


void write_to_history(char **command) {
  FILE *fp = fopen(get_history_path(), "a");
  assert(fp != NULL);

  for (int i = 0; command[i] != NULL; i++) {
    fputs(command[i], fp);
    if (command[i+1] != NULL) {
      // don't add space after the word if it's the last word.
      fputc(' ', fp);
    }
  }

  fputc('\n', fp);

  fclose(fp);
  return;
}


int get_nlines() {
  FILE *fp = fopen(get_history_path(), "r");
  assert(fp != NULL);

  int nlines = 0;
  int ch;
  while ((ch = fgetc(fp)) != EOF) {
    if (ch == '\n') nlines++;
  }
  fclose(fp);
  return nlines;
}


// Get the line number that the history command starts on
// printing the .cowrie_history file.
static int get_starting_line_number(char *asciiNumber, int nlines) {
  int startingline;
  if (asciiNumber != NULL) {
    if (atoi(asciiNumber) > 0) {
      startingline = nlines - atoi(asciiNumber);

    } else {
      return -1;
    }
  } else {
    startingline = nlines - 10;
  }
  return (startingline > 0) ? startingline: 0;
}

#include <stdio.h>
void green () {
  printf("\033[1;32m");
}

void blue () {
  printf("\033[1;34m");
}

void reset () {
  printf("\033[0m");
}

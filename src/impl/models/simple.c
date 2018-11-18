#include <stdio.h>
#include <stdlib.h>

int main (int argc, char ** argv) {
  if (argc != 3) {
    printf("Usage: {NAME} {INPUT_FILE}\n");
    exit(1);
  }

  // Open files
  FILE *input = fopen(argv[2], "rb");
  if (!input) perror(argv[2]), exit(1);

  int c;
  printf("ssso!\n");
  printf("%d\n", getc(input));
  while((c=getc(input))!=EOF) {
    printf("%d\n", c);
  }
}

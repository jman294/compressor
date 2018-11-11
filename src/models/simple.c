int main (int argc, char ** argv) {
  FILE* =
  if (argc!=3) {
    printf("Usage: {NAME} {INPUT_FILE}");
    exit(1);
  }

  // Open files
  FILE *input = fopen(argv[2], "wb");
  if (!output) perror(argv[3]), exit(1);

  int c;
  while((c=getc(input))!=EOF) {
    printf("%d", c);
  }
}

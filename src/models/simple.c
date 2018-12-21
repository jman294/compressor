#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "model.h"

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')

int main (int argc, char ** argv) {
  ModelData_t contextCount = {0};
  ModelData_t oneCount = {0};
  ModelData_t predictions = {0};

  context context = 0;

  if (argc != 3) {
    printf("Usage: {NAME} {INPUT_FILE}\n");
    exit(1);
  }

  // Open files
  FILE *input = fopen(argv[2], "rb");
  if (!input) perror(argv[2]), exit(1);

  int c;

  for (int i=0; i<sizeof(context); ++i) {
    int c=getc(input);
    if (c==EOF) c=0;
    context=(context<<8)+(c&0xff);
  }
  while ((c=getc(input))!=EOF) {
    for (int i = 7; i >= 0; i--) {
      int nextBit = (c >> i) & 1;
      contextCount[context] = contextCount[context] + 1;
      if (nextBit == 1) {
        oneCount[context] = oneCount[context] + 1;
      }

      context = (context << 1) | nextBit;
    }
  }
  for (int i = 0; i < NUM_CONTEXTS; i++) {
    if (contextCount[i] != 0) {
      predictions[i] = (MODEL_LIMIT * oneCount[i]) / contextCount[i];
    }
    printf("%d %d %d %d\n", i, oneCount[i], contextCount[i], predictions[i]);
  }
  for (int i = 0; i < NUM_CONTEXTS; i++) {
    printf("%d, ", predictions[i]);
  }
}

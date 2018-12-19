#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>

#include "util.h"
#include "modelenum.h"
#include "decompressor.h"
#include "decompressorpredictor.h"

int decode (DecompressorPredictor * p, uint32_t* x1, uint32_t* x2, uint32_t* x, int prediction, FILE* archive, FILE* output, short changeInterval) {
  // Update the range
  const uint32_t xmid = (*x1) + (((*x2)-(*x1)) >> 12) * prediction;
  assert(xmid >= (*x1) && xmid < (*x2));
  int y=0;
  if ((*x)<=xmid) {
    y=1;
    (*x2)=xmid;
  }
  else
    (*x1)=xmid+1;
  DP_Update(p, y);
  printf("%d %d\n", y, prediction);

  // Shift equal MSB's out
  while ((((*x1)^(*x2))&0xff000000)==0) {
    (*x1)<<=8;
    (*x2)=((*x2)<<8)+255;
    int c=getc(archive);
    if (c==EOF) c=0;
    (*x)=((*x)<<8)+c;
  }
  return y;
}

void readHeaderInit (FILE* input, int * startingCode, uint32_t * headerLength) {
  fread(headerLength, sizeof(uint32_t), 1, input);
  /**headerLength = *headerLength & getc(input);*/
  /**headerLength = *headerLength & (getc(input) << 8);*/
  /**headerLength = *headerLength & (getc(input) << 16);*/
  /**headerLength = *headerLength & (getc(input) << 24);*/
  *startingCode = (int)getc(input);
}

void decompress (FILE* input, FILE* output, DecompressorPredictor* p) {
  uint32_t x1 = 0;
  uint32_t x2 = 0xffffffff;
  uint32_t x = 0;

  int startingCode;
  uint32_t headerLength;
  readHeaderInit(input, &startingCode, &headerLength);

  FILE *header = fdopen(dup(fileno(input)), "rb");

  DP_SelectModel(p, startingCode);
  /*printf("%lx %d\n", headerLength, startingCode);*/

  uint32_t bitCount = 8;

  // Reads in first 4 bytes into x
  fseek(input, headerLength, SEEK_SET);
  for (int i=0; i<4; ++i) {
    int c=getc(input);
    if (c==EOF) c=0;
    x=(x<<8)+(c&0xff);
  }

  int headerPos = 5;
  int changeInterval = 128; // Has to be synced with compressor's change interval

  fseek(header, headerPos, SEEK_SET);
  int run = 1;
  while (run) {
    if (bitCount % (changeInterval * 8) == 0) {
        int modelCode = getc(header);

        printf("MODEL %d %lx\n", modelCode, ftell(header));
        DP_SelectModel(p, modelCode);

        bitCount = 0;
    }
    run = !decode(p, &x1, &x2, &x, DP_Predict(p), input, output, changeInterval);
    if (!run) {
      break;
    }
    int c=1;
    // Decode until you reach a byte
    while (c<128) {
      c+=c+decode(p, &x1, &x2, &x, DP_Predict(p), input, output, changeInterval);
      bitCount += 1;
    }
    bitCount += 1;
    // c started at 1. You have to remove it from the output because it was not 0, and the 1 sticks to the front of the decoded byte. Hence the subtraction.
    putc(c-128, output);
  }

  fclose(input);
  fclose(output);
}

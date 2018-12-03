#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

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

  // Shift equal MSB's out
  while ((((*x1)^(*x2))&0xff000000)==0) {
    (*x1)<<=8;
    (*x2)=((*x2)<<8)+255;
    int c=getc(archive);
    if (ftell(archive) % changeInterval == 0) {
      printf("%ld %ld\n", ftell(archive), ftell(output));
    }
    if (c==EOF) c=0;
    (*x)=((*x)<<8)+c;
  }
  return y;
}

int readHeader (FILE* input) {
  return (int)getc(input);
}

void decompress (FILE* input, FILE* output, DecompressorPredictor* p) {
  uint32_t x1 = 0;
  uint32_t x2 = 0xffffffff;
  uint32_t x = 0;

  int startingCode = readHeader(input);
  Model *m = malloc(sizeof(*m));
  MO_New(m, startingCode);
  DP_SelectModel(p, m);

  // Reads in first 4 bytes into x
  for (int i=0; i<4; ++i) {
    int c=getc(input);
    if (c==EOF) c=0;
    x=(x<<8)+(c&0xff);
  }

  int changeInterval = 128; // Has to be synced with compressor's change interval

  while (!decode(p, &x1, &x2, &x, DP_Predict(p), input, output, changeInterval)) {
    int c=1;
    // Decode until you reach a byte
    while (c<128) {
      c+=c+decode(p, &x1, &x2, &x, DP_Predict(p), input, output, changeInterval);
    }
    // c started at 1. You have to remove it from the output because it was not 0, and the 1 sticks to the front of the decoded byte. Hence the subtraction.
    putc(c-128, output);
  }
  printf("donzo\n");

  fclose(input);
  fclose(output);
}

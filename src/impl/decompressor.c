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
    if (c==EOF) c=0;
    (*x)=((*x)<<8)+c;
  }
  return y;
}

void readHeaderInit (FILE* input, int * startingCode, unsigned long * headerLength) {
  *startingCode = (int)getc(input);
  *headerLength = (int)getc(input);
}

void readHeaderData (FILE* input, uint8_t * dataArray[], unsigned long length) {
  fseek(input, 2, SEEK_SET);
  for (int i = 0; i < length - 2; i++) {
    uint8_t byte = getc(input);
    (*dataArray)[i] = byte;
  }
}

void decompress (FILE* input, FILE* output, DecompressorPredictor* p) {
  uint32_t x1 = 0;
  uint32_t x2 = 0xffffffff;
  uint32_t x = 0;

  int startingCode;
  unsigned long headerLength;
  readHeaderInit(input, &startingCode, &headerLength);
  uint8_t modelSwitches[4] = {0, 1, 1, 1};
  FILE *header = fdopen (dup (fileno (input)), "rb");

  /*memset(modelSwitches, 0, (headerLength-2)*sizeof(uint8_t));*/
  /*readHeaderData(input, &modelSwitches, headerLength);*/

  Model *m = malloc(sizeof(*m));
  MO_New(m, startingCode);
  DP_SelectModel(p, m);

  unsigned long bitCount = 0;

  // Reads in first 4 bytes into x
  fseek(input, headerLength, SEEK_SET);
  for (int i=0; i<4; ++i) {
    int c=getc(input);
    if (c==EOF) c=0;
    x=(x<<8)+(c&0xff);
  }

  int changeInterval = 128; // Has to be synced with compressor's change interval
  int headerPos = 0;

  fseek(header, 2, SEEK_SET);
  int run = 1;
  while (run) {
    if (bitCount % (changeInterval * 8) == 0) {
      int modelCode = getc(header);
      printf("MODEL SWITCH %d %ld %ld\n", modelCode, ftell(input), ftell(header));
      headerPos += 1;

      Model *m = malloc(sizeof(*m));
      MO_New(m, modelCode);
      DP_SelectModel(p, m);

      bitCount = 0;
    }
    run = !decode(p, &x1, &x2, &x, DP_Predict(p), input, output, changeInterval);
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
  printf("donzo\n");

  fclose(input);
  fclose(output);
}

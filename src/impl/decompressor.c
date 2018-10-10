#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "util.h"
#include "decompressor.h"
#include "decompressorpredictor.h"

int decode (uint32* x1, uint32* x2, uint32* x, int prediction, FILE* archive, short changeInterval) {
  // Update the range
  const uint32 xmid = (*x1) + (((*x2)-(*x1)) >> 12) * prediction;
  assert(xmid >= (*x1) && xmid < (*x2));
  int y=0;
  if ((*x)<=xmid) {
    y=1;
    (*x2)=xmid;
  }
  else
    (*x1)=xmid+1;

  // Shift equal MSB's out
  while ((((*x1)^(*x2))&0xff000000)==0) {
    (*x1)<<=8;
    (*x2)=((*x2)<<8)+255;
    int c=getc(archive);
    if (ftell(archive) % changeInterval == 0) {
      printf("%ld\n", ftell(archive));
      getc(archive);
    }
    if (c==EOF) c=0;
    (*x)=((*x)<<8)+c;
  }
  return y;
}

void decompress (FILE* input, FILE* output, DecompressorPredictor* p) {
  uint32 x1 = 0;
  uint32 x2 = 0xffffffff;
  uint32 x = 0;

  // Reads in first 4 bytes into x
  for (int i=0; i<4; ++i) {
    int c=getc(input);
    if (c==EOF) c=0;
    x=(x<<8)+(c&0xff);
  }

  int changeInterval = 128; // Has to be synced with compressor's change interval

  while (!decode(&x1, &x2, &x, DP_Predict(p), input, changeInterval)) {
    int c=1;
    while (c<256) {
      c+=c+decode(&x1, &x2, &x, DP_Predict(p), input, changeInterval);
    }
    putc(c-256, output);
  }

  fclose(input);
  fclose(output);
}

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "util.h"
#include "decompressor.h"
#include "decompressorpredictor.h"

int decode (uint32* x1, uint32* x2, uint32* x, int prediction, FILE* archive) {

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
    if (c==EOF) c=0;
    (*x)=((*x)<<8)+c;
  }
  return y;
}

void decompress (FILE* input, FILE* output, DecompressorPredictor* p) {
  uint32 x1 = 0;
  uint32 x2 = 0xffffffff;
  uint32 x = 0;

  int cxt;  // Context: last 0-8 bits with a leading 1
  int ct[512][2];  // 0 and 1 counts in context cxt
  memset(ct, 0, sizeof(ct));

  for (int i=0; i<4; ++i) {
    int c=getc(input);
    if (c==EOF) c=0;
    x=(x<<8)+(c&0xff);
  }

  while (!decode(&x1, &x2, &x, DP_Predict(p), input)) {
    int c=1;
    while (c<256)
      c+=c+decode(&x1, &x2, &x, DP_Predict(p), input);
    putc(c-256, output);
  }

  fclose(input);
  fclose(output);
}

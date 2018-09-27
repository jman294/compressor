#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "compressor.h"
#include "compressorpredictor.h"

int prediction (int cxt, int ct[512][2]) {
  return 4096*(ct[cxt][1]+1)/(ct[cxt][0]+ct[cxt][1]+2);
}

void encode (uint32* x1, uint32* x2, int y, FILE* archive, int prediction) {
  // Update the range
  const uint32 xmid = *x1 + ((*x2-*x1) >> 12) * prediction;
  assert(xmid >= *x1 && xmid < *x2);
  if (y)
    *x2=xmid;
  else
    *x1=xmid+1;

  // Shift equal MSB's out
  while (((*x1^*x2)&0xff000000)==0) {
    putc(*x2>>24, archive);
    *x1<<=8;
    *x2=(*x2<<8)+255;
  }
}

void flush (uint32* x1, uint32* x2, FILE* archive) {
  while (((*x1^*x2)&0xff000000)==0) {
    putc(*x2>>24, archive);
    *x1<<=8;
    *x2=(*x2<<8)+255;
  }
  putc(*x2>>24, archive);  // First unequal byte
}

void compress (FILE* input, FILE* output, CompressorPredictor* p) {
  uint32 x1 = 0;
  uint32 x2 = 0xffffffff;

  int ct[512][2];  // 0 and 1 counts in context cxt
  memset(ct, 0, sizeof(ct));

  int c;

  while ((c=getc(input))!=EOF) {
    encode(&x1, &x2, 0, output, CP_Predict(p));
    for (int i=7; i>=0; --i)
      encode(&x1, &x2, (c>>i)&1, output, CP_Predict(p));
  }
  encode(&x1, &x2, 1, output, CP_Predict(p));  // EOF code
  flush(&x1, &x2, output);

  fclose(output);
  fclose(input);
}

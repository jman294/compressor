#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "compressor.h"
#include "compressorpredictor.h"
#include "util.h"

void encode (uint32_t* x1, uint32_t* x2, int y, FILE* archive, int prediction, short changeInterval, int code) {
  // Update the range
  const uint32_t xmid = *x1 + ((*x2-*x1) >> 12) * prediction;
  assert(xmid >= *x1 && xmid < *x2);
  if (y)
    *x2=xmid;
  else
    *x1=xmid+1;

  // Shift equal MSB's out
  while (((*x1^*x2)&0xff000000)==0) {
    putc(*x2>>24, archive);
    if (ftell(archive) % changeInterval == 0) {
      putc(code, archive);
    }
    *x1<<=8;
    *x2=(*x2<<8)+255;
  }
}

void compress (FILE* input, FILE* output, CompressorPredictor* p) {
  uint32_t x1 = 0;
  uint32_t x2 = 0xffffffff;

  short changeInterval = 128; // Every 128 bytes, we change models

  int c;

  int code = CP_GetBestModel(p)->code;
  while ((c=getc(input))!=EOF) {
    code = CP_GetBestModel(p)->code;
    encode(&x1, &x2, 0, output, CP_Predict(p), changeInterval, code);
    for (int i=7; i>=0; --i)
      encode(&x1, &x2, (c>>i)&1, output, CP_Predict(p), changeInterval, code);
  }
  encode(&x1, &x2, 1, output, CP_Predict(p), changeInterval, code);  // EOF code
  flush(&x1, &x2, output);

  fclose(output);
  fclose(input);
}

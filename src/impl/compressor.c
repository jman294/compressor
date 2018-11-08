#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "compressor.h"
#include "compressorpredictor.h"
#include "util.h"
#include "modelenum.h"

void encode (CompressorPredictor * p, uint32_t* x1, uint32_t* x2, int y, FILE* archive, int prediction, short changeInterval, int code) {
  // Update the range
  const uint32_t xmid = *x1 + ((*x2-*x1) >> 12) * prediction;
  assert(xmid >= *x1 && xmid < *x2);
  if (y)
    *x2=xmid;
  else
    *x1=xmid+1;
  /*printf("%d\n", y);*/
  CP_Update(p, y);

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

void writeHeader (FILE* archive, int startingCode) {
  putc(startingCode, archive);
}

void compress (FILE* input, FILE* output, CompressorPredictor* p) {
  uint32_t x1 = 0;
  uint32_t x2 = 0xffffffff;

  short changeInterval = 128; // Every 128 bytes, we change models

  int c;

  int code;
  writeHeader(output, TEXT1); // 1 Is Starting model. Can be picked intelligently
  while ((c=getc(input))!=EOF) {
    code = CP_GetBestModel(p)->code;
    encode(p, &x1, &x2, 0, output, CP_Predict(p), changeInterval, code);
    printf("%d\n", CP_GetBestModel(p)->code);
    /*printf("%f\t%f\t%f\n", CP_GetBestModel(p)->score, (*p->models)[1]->score, (*p->models)[0]->score);*/
    /*printf("%d\t%d\n", MO_GetPrediction((*p->models)[1], p->ctx%5), MO_GetPrediction((*p->models)[0], p->ctx%5));*/
    /*printf("%d\t%d\n", (*p->models)[1]->lastPrediction, (*p->models)[0]->lastPrediction);*/
    for (int i=7; i>=0; --i)
      encode(p, &x1, &x2, (c>>i)&1, output, CP_Predict(p), changeInterval, code);
  }
  encode(p, &x1, &x2, 1, output, CP_Predict(p), changeInterval, code);  // EOF code
  flush(&x1, &x2, output);

  fclose(output);
  fclose(input);
}

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "compressor.h"
#include "compressorpredictor.h"
#include "util.h"
#include "modelenum.h"

void encode (CompressorPredictor * p, uint32_t* x1, uint32_t* x2, int y, FILE* archive, FILE* input, int prediction, short changeInterval) {
  // Update the range
  const uint32_t xmid = *x1 + ((*x2-*x1) >> 12) * prediction;
  assert(xmid >= *x1 && xmid < *x2);
  if (y)
    *x2=xmid;
  else
    *x1=xmid+1;
  CP_Update(p, y);

  // Shift equal MSB's out
  while (((*x1^*x2)&0xff000000)==0) {
    putc(*x2>>24, archive);
    *x1<<=8;
    *x2=(*x2<<8)+255;
  }
}

void writeHeader (FILE* archive, int startingCode, unsigned long headerLength) {
  rewind(archive);
  putc(startingCode, archive);
  putc(headerLength, archive);
}

void compress (FILE* input, FILE* output, CompressorPredictor* p) {
  int startingCode = p->currentModel->code;
  uint32_t x1 = 0;
  uint32_t x2 = 0xffffffff;

  int changeInterval = 128;

  unsigned long headerPos = 2;
  unsigned long headerLength = 3; // This is only for testing purposes

  fseek(output, headerLength, SEEK_SET);
  int c;
  while ((c=getc(input))!=EOF) {
    if (ftell(input) % changeInterval == 0) {
      int modelCode = CP_GetBestModel(p)->code;
      Model *m = malloc(sizeof(*m));
      MO_New(m, modelCode);
      CP_SelectModel(p, m);
      fseek(output, headerPos, SEEK_SET);
      putc(modelCode, output);
      headerPos += 1;
      fseek(output, 0, SEEK_END);
      if (headerLength > ftell(output)) {
        fseek(output, headerLength, SEEK_SET);
      }
      printf("%ld %ld %d\n", ftell(output), ftell(input), modelCode);
    }
    /*encode(p, &x1, &x2, 0, output, CP_Predict(p), changeInterval, code);*/
    for (int i=7; i>=0; --i) {
      encode(p, &x1, &x2, (c>>i)&1, output, input, CP_Predict(p), changeInterval);
    }
  }
  encode(p, &x1, &x2, 1, output, input, CP_Predict(p), changeInterval);  // EOF code
  flush(&x1, &x2, output);

  writeHeader(output, startingCode, headerLength); // Can be picked intelligently

  printf("--------- %ld %x\n", headerPos, getc(output));
  fclose(output);
  fclose(input);
}

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
    if (ftell(archive) % 128 == 0) {
      int modelCode = CP_GetBestModel(p)->code;
      unsigned char markByte = /*(modelCode << 7) | */(ftell(input) % 128);
      printf("asdlfkasldkfj %ld %ld %ld\n", ftell(archive), ftell(input), markByte & 0b01111111 + ftell(input));
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

  writeHeader(output, p->currentModel->code); // 1 Is Starting model. Can be picked intelligently

  bool outputInFour = false;
  short tempByteCount = 0;

  int c;
  while ((c=getc(input))!=EOF) {
    /*if (outputInFour) {*/
      /*if (tempByteCount >= 3) {*/
        /*printf("asdlfkasldkfj %ld\n", ftell(output));*/
        /*tempByteCount = 0;*/
        /*outputInFour = false;*/
      /*} else {*/
        /*tempByteCount += 1;*/
      /*}*/
    /*}*/
    /*encode(p, &x1, &x2, 0, output, CP_Predict(p), changeInterval, code);*/
    for (int i=7; i>=0; --i) {
      encode(p, &x1, &x2, (c>>i)&1, output, input, CP_Predict(p), changeInterval);
    }
  }
  encode(p, &x1, &x2, 1, output, input, CP_Predict(p), changeInterval);  // EOF code
  flush(&x1, &x2, output);

  fclose(output);
  fclose(input);
  printf("---------\n");
}

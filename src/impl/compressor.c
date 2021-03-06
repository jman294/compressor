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
  /*printf("%d\n", prediction);*/

  // Shift equal MSB's out
  while (((*x1^*x2)&0xff000000)==0) {
    putc(*x2>>24, archive);
    *x1<<=8;
    *x2=(*x2<<8)+255;
  }
}

void writeHeader (FILE* archive, int startingCode, uint32_t headerLength) {
  rewind(archive);
  fwrite(&headerLength, sizeof(uint32_t), 1, archive);
  putc(startingCode, archive);
}

void compress (FILE* input, FILE* output, CompressorPredictor* p) {
  int startingCode = p->currentModel->code;
  CP_SelectModel(p, startingCode);
  uint32_t x1 = 0;
  uint32_t x2 = 0xffffffff;

  int changeInterval = 128;

  uint32_t headerPos = 5;
  fseek(input, 0, SEEK_END);
  uint32_t headerLength = ftell(input)/changeInterval + headerPos; // This is only for testing purposes
  fseek(input, 0, SEEK_SET);
  printf("%d %d\n", startingCode, headerLength);

  uint32_t bitCount = 8;

  fseek(output, headerLength, SEEK_SET);
  int c;
  while ((c=getc(input))!=EOF) {
    if (bitCount % (changeInterval * 8) == 0) {
      int modelCode = CP_GetBestModel(p)->code;
      /*int modelCode = 1;*/
      CP_SelectModel(p, modelCode);
      fseek(output, headerPos, SEEK_SET);
      putc(modelCode, output);
      headerPos += 1;
      fseek(output, 0, SEEK_END);
      if (headerLength > ftell(output)) {
        fseek(output, headerLength, SEEK_SET);
      }
      bitCount = 0;
    }
    for (int i=7; i>=0; --i) {
      encode(p, &x1, &x2, (c>>i)&1, output, input, CP_Predict(p), changeInterval);
      bitCount += 1;
    }
  }
  encode(p, &x1, &x2, 1, output, input, CP_Predict(p), changeInterval);  // EOF code
  flush(&x1, &x2, output);

  printf("Compression level: %f%%\n", (((float) ftell(input))-((float) ftell(output)))/ftell(input)*100);

  writeHeader(output, startingCode, headerLength); // Can be picked intelligently

  fclose(output);
  fclose(input);
}

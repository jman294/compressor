#include <stdlib.h>

#include "util.h"
#include "compressorpredictor.h"
#include "model.h"
#include "modelenum.h"

void CP_New (CompressorPredictor * cp, ModelArray_t mos, context ctx) {
  cp->ctx = ctx;
}

int CP_Predict (CompressorPredictor * cp) {
  return 200;
}

void CP_Update (CompressorPredictor * cp, int bit) {
  cp->ctx = (cp->ctx << 1) | bit;
}

Model * CP_GetBestModel (CompressorPredictor * cp) {
  Model * m = malloc(sizeof(Model));
  MO_New(m, TEXT2);
  return m;
}

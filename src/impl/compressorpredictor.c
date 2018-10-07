#include "util.h"
#include "compressorpredictor.h"

void CP_New (CompressorPredictor * cp, Model models[2], context ctx) {
  cp->ctx = ctx;
}

int CP_Predict (CompressorPredictor * cp) {
  return 200;
}

int CP_Update (CompressorPredictor * cp, int bit) {
  cp->ctx = (cp->ctx << 1) | bit;
}

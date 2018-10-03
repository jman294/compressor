#include "compressorpredictor.h"

void CP_New (CompressorPredictor * cp, Model models[10]) {
  cp->ctx = 2000;
}

int CP_Predict (CompressorPredictor * cp) {
  return cp->ctx;
}

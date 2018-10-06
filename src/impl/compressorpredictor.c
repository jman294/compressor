#include "compressorpredictor.h"

void CP_New (CompressorPredictor * cp, Model models[2]) {
  cp->ctx = 2000;
}

int CP_Predict (CompressorPredictor * cp) {
  return 200;
}

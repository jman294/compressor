#include "decompressorpredictor.h"
#include "model.h"

void DP_New (DecompressorPredictor * dp, ModelArray_t mos, context ctx) {
  dp->ctx = 2000;
}

int DP_Predict (DecompressorPredictor * dp) {
  return 200;
}

void DP_SelectModel (DecompressorPredictor * dp, Model * m) {
  dp->m = m;
}

#include "decompressorpredictor.h"
#include "model.h"

void DP_New (DecompressorPredictor * dp, ModelArray_t mos, context ctx) {
  dp->ctx = 0;
}

int DP_Predict (DecompressorPredictor * dp) {
  return 200;
}

void DP_SelectModel (DecompressorPredictor * dp, Model * m) {
  dp->m = m;
}

void DP_Update (DecompressorPredictor * dp, int bit) {
  printf("%x", dp->ctx);
  dp->ctx = (dp->ctx << 1) | bit;
}

#include "decompressorpredictor.h"

void DP_New (DecompressorPredictor * dp) {
  dp->ctx = 2000;
}

int DP_Predict (DecompressorPredictor * dp) {
  return dp->ctx;
}

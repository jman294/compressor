#include <assert.h>

#include "decompressorpredictor.h"
#include "model.h"

void DP_New (DecompressorPredictor * dp, ModelArray_t mos, int modelCount, context ctx) {
  dp->modelCount = modelCount;
  dp->ctx = 0;
  dp->models = mos;
  dp->predictionCount = 0;
}

int DP_Predict (DecompressorPredictor * dp) {
  assert(dp->currentModel != NULL);
  dp->predictionCount += 1;
  return MO_GetPrediction(dp->currentModel, dp->ctx);
}

void DP_Update (DecompressorPredictor * dp, int bit) {
  dp->ctx = (dp->ctx << 1) | bit;
}

void DP_SelectModel (DecompressorPredictor * dp, Model * m) {
  dp->predictionCount = 0;
  dp->currentModel = m;
}

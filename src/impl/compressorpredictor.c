#include <stdlib.h>
#include <math.h>

#include "util.h"
#include "compressorpredictor.h"
#include "model.h"
#include "modelenum.h"

void CP_New (CompressorPredictor * cp, ModelArray_t mos, int modelCount, context ctx) {
  cp->ctx = ctx;
  cp->models = mos;
  cp->modelCount = modelCount;
}

int CP_Predict (CompressorPredictor * cp) {
  if (cp->models != NULL) {
    for (int i = 0; i < cp->modelCount; i++) {
      Model * currentModel = (*cp->models)[i];
      currentModel->lastPrediction = MO_GetPrediction(currentModel, cp->ctx);
    }
  }
  int prediction = MO_GetPrediction(cp->currentModel, cp->ctx);
  return prediction;
}

void CP_Update (CompressorPredictor * cp, int bit) {
    for (int i = 0; i < cp->modelCount; i++) {
      Model * currentModel = (*cp->models)[i];
      currentModel->score = ((1 - fabs(bit - ((float)currentModel->lastPrediction/((float)MODEL_LIMIT))) * 0.55) + (.45 * currentModel->score))/2;
    }
  cp->ctx = (cp->ctx << 1) | bit;
}

void CP_SelectModel (CompressorPredictor * cp, Model * m) {
  cp->currentModel = m;
}

Model * CP_GetBestModel (CompressorPredictor * cp) {
  Model * bestScore = (*cp->models)[0];
  for (int i = 0; i < cp->modelCount; i++) {
    if ((*cp->models)[i]->score > bestScore->score) {
      bestScore = (*cp->models)[i];
    }
  }
  return bestScore;
}

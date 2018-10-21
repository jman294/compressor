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
  if (mos != NULL) {
    cp->currentModel = (*mos)[0];
  }
}

int CP_Predict (CompressorPredictor * cp) {
  if (cp->models != NULL) {
    for (int i = 0; i < cp->modelCount; i++) {
      Model * currentModel = (*cp->models)[i];
      currentModel->lastPrediction = MO_GetPrediction(currentModel, cp->ctx);
    }
  }
  return MO_GetPrediction(cp->currentModel, cp->ctx);
}

void CP_Update (CompressorPredictor * cp, int bit) {
    for (int i = 0; i < cp->modelCount; i++) {
      Model * currentModel = (*cp->models)[i];
      /*printf("%f", pow(1 - fabs(bit - (float)currentModel->lastPrediction/((float)MODEL_LIMIT)), .5));*/
      currentModel->score = ((1 - fabs(bit - ((float)currentModel->lastPrediction/((float)MODEL_LIMIT))) * 0.55) + (.45 * currentModel->score))/2;
      printf("%f", currentModel->score);
    }
  cp->ctx = (cp->ctx << 1) | bit;
}

void CP_SelectModel (CompressorPredictor * cp, Model * m) {
  cp->currentModel = m;
}

Model * CP_GetBestModel (CompressorPredictor * cp) {
  Model * m = malloc(sizeof(Model));
  MO_New(m, TEXT2);
  return m;
}

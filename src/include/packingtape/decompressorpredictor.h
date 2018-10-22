#ifndef DECOMPRESSORPREDICTOR_H_   /* Include guard */
#define DECOMPRESSORPREDICTOR_H_

#include "model.h"

typedef struct DecompressorPredictor {
  int ctx;
  int modelCount;
  ModelArray_t models;
  Model * currentModel;
} DecompressorPredictor;

void DP_New (DecompressorPredictor * dp, ModelArray_t mos, int modelCount, context ctx);

int DP_Predict (DecompressorPredictor * dp);

void DP_Update (DecompressorPredictor * dp, int bit);

void DP_SelectModel (DecompressorPredictor * dp, Model * m);

#endif // DECOMPRESSORPREDICTOR_H_

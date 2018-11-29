#ifndef COMPRESSORPREDICTOR_H_   /* Include guard */
#define COMPRESSORPREDICTOR_H_

#include "util.h"
#include "model.h"

typedef struct CompressorPredictor {
  context ctx;
  ModelArray_t models;
  int modelCount;
  Model * currentModel;
} CompressorPredictor;

void CP_New (CompressorPredictor * cp, ModelArray_t mos, int modelCount, context ctx);

int CP_Predict (CompressorPredictor * cp);

void CP_Update (CompressorPredictor * cp, int bit);

void CP_UpdateCtx (CompressorPredictor * cp, int bit);

void CP_SelectModel (CompressorPredictor * cp, Model * m);

Model * CP_GetBestModel(CompressorPredictor * cp);

#endif // COMPRESSORPREDICTOR_H_

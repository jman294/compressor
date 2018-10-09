#ifndef COMPRESSORPREDICTOR_H_   /* Include guard */
#define COMPRESSORPREDICTOR_H_

#include "util.h"
#include "model.h"

typedef struct CompressorPredictor {
  context ctx;
  Model models[2];
} CompressorPredictor;

void CP_New (CompressorPredictor * cp, Model models[2], context ctx);

int CP_Predict (CompressorPredictor * cp);

void CP_Update (CompressorPredictor * cp, int bit);

void CP_SelectModel (CompressorPredictor * cp, Model m);

Model * CP_GetBestModel(CompressorPredictor * cp);

#endif // COMPRESSORPREDICTOR_H_

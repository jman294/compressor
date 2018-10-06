#ifndef COMPRESSORPREDICTOR_H_   /* Include guard */
#define COMPRESSORPREDICTOR_H_

#include "model.h"

typedef struct CompressorPredictor {
  int ctx;
  Model models[2];
} CompressorPredictor;

void CP_New (CompressorPredictor * cp, Model models[2]);

int CP_Predict (CompressorPredictor * cp);

int CP_Update (CompressorPredictor * cp, int bit);

void CP_SelectModel (CompressorPredictor * cp, Model m);

Model CP_BestModel(CompressorPredictor * cp);

#endif // COMPRESSORPREDICTOR_H_

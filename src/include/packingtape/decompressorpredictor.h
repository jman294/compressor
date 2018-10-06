#ifndef DECOMPRESSORPREDICTOR_H_   /* Include guard */
#define DECOMPRESSORPREDICTOR_H_

#include "model.h"

typedef struct DecompressorPredictor {
  int ctx;
} DecompressorPredictor;

void DP_New (DecompressorPredictor * dp);

int DP_Predict (DecompressorPredictor * dp);

void DP_Update (DecompressorPredictor * dp);

void DP_SelectModel (DecompressorPredictor * dp, Model m);

#endif // DECOMPRESSORPREDICTOR_H_

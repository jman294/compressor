#ifndef DECOMPRESSORPREDICTOR_H_   /* Include guard */
#define DECOMPRESSORPREDICTOR_H_

typedef struct DecompressorPredictor {
  int ctx;
} DecompressorPredictor;

void DP_New (DecompressorPredictor * dp);

int DP_Predict (DecompressorPredictor * dp);

int DP_Update (DecompressorPredictor * dp);

#endif // DECOMPRESSORPREDICTOR_H_

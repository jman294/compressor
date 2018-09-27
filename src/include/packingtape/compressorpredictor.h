#ifndef COMPRESSORPREDICTOR_H_   /* Include guard */
#define COMPRESSORPREDICTOR_H_

typedef struct CompressorPredictor {
  int ctx;
} CompressorPredictor;

void CP_New (CompressorPredictor * cp);

int CP_Predict (CompressorPredictor * cp);

int CP_Update (CompressorPredictor * cp);

#endif // COMPRESSORPREDICTOR_H_

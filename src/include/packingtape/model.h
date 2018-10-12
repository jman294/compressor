#ifndef MODEL_H_   /* Include guard */
#define MODEL_H_

#include "util.h"

typedef struct Model {
  int code;
} Model;

void S_MO_EnumerateAllModels (Model * (*mos)[2]);

void MO_New (Model * m, int code);

int MO_GetPrediction (Model * m, context context);

#endif // MODEL_H_

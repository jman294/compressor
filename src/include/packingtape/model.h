#ifndef MODEL_H_   /* Include guard */
#define MODEL_H_

#include "util.h"

#define MODEL_LIMIT 4095

typedef struct Model {
  int code;
  int data;
  int lastPrediction;
  float score;
} Model;

typedef Model * (*ModelArray_t)[2];

void S_MO_EnumerateAllModels (ModelArray_t mos); // A pointer to an array that contains pointers to Models

void MO_New (Model * m, int code);

int MO_GetPrediction (Model * m, context context);

#endif // MODEL_H_

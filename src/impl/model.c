#include <stdlib.h>

#include "model.h"
#include "modelenum.h"
#include "util.h"

void S_MO_EnumerateAllModels (ModelArray_t mos) {
  // NOTE This is an enumeration of all existing models
  Model * text1 = malloc(sizeof(*text1));
  MO_New(text1, TEXT1);
  Model * text2 = malloc(sizeof(*text2));
  MO_New(text2, TEXT2);
  (*mos)[0] = text1;
  (*mos)[1] = text2;
}

void MO_New (Model * m, int code) {
  m->code = code;
  m->score = 0;

  // NOTE This is an enumeration of model by their code
  switch (code) {
    case TEXT1:
      MO_SetData(m, &TEXT1_Data);
      break;
    case TEXT2:
      MO_SetData(m, &TEXT2_Data);
      break;
    default:
      MO_SetData(m, NULL);
  }
}

void MO_SetData (Model * m, const ModelData_t * data) {
  m->data = data;
}

int MO_GetPrediction (Model * m, context context) {
  return (*m->data)[context];
}

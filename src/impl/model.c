#include <stdlib.h>

#include "model.h"
#include "modelenum.h"
#include "util.h"

void S_MO_EnumerateAllModels (ModelArray_t mos) {
  Model * text1 = malloc(sizeof(*text1));
  MO_New(text1, TEXT1);
  MO_SetData(text1, &TEXT1_Data);
  Model * text2 = malloc(sizeof(*text2));
  MO_New(text2, TEXT2);
  MO_SetData(text2, &TEXT2_Data);
  (*mos)[0] = text1;
  (*mos)[1] = text2;
}

void MO_New (Model * m, int code) {
  m->code = code;
  m->score = 0;
  m->data = NULL;
  switch (code) {
    case TEXT1:
      m->data = &TEXT1_Data;
      break;
    case TEXT2:
      m->data = &TEXT2_Data;
      break;
  }
}

void MO_SetData (Model * m, const ModelData_t * data) {
  m->data = data;
}

int MO_GetPrediction (Model * m, context context) {
  return 200;
  /*return (*m->data)[context];*/
}

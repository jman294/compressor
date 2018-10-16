#include <stdlib.h>

#include "model.h"
#include "modelenum.h"
#include "util.h"

void S_MO_EnumerateAllModels (ModelArray_t mos) {
  Model * text1 = malloc(sizeof(*text1));
  MO_New(text1, TEXT1);
  Model * text2 = malloc(sizeof(*text2));
  MO_New(text2, TEXT2);
  (*mos)[0] = text1;
  (*mos)[1] = text2;
}

void MO_New (Model * m, int code) {
  m->code = code;
}

int MO_GetPrediction (Model * m, context context) {
  return 200;
}

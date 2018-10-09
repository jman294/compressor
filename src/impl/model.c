#include "model.h"
#include "util.h"

void MO_New (Model * m, int code) {
  m->code = code;
}

int MO_GetPrediction (Model * m, context context) {
  return 200;
}

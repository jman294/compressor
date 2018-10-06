#include "model.h"

void MO_New (Model * m, int code) {
  m->code = code;
}

int MO_GetPrediction (Model * m, int context) {
  return 200;
}

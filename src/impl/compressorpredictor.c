#include "compressorpredictor.h"

void CP_New (CompressorPredictor * cp) {
  cp->ctx = 2000;
}

int CP_Predict (CompressorPredictor * cp) {
  return cp->ctx;
}

/*void CP_Update (CompressorPredictor * cp) {*/
  /*cp->ctx = cp->ctx*/



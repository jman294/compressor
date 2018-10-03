#include <stdlib.h>

#include "packingtape/compressor.h"
#include "packingtape/compressorpredictor.h"
#include "packingtape/decompressor.h"
#include "packingtape/decompressorpredictor.h"

int main () {
  FILE* input = fopen("../LICENSE", "rb");
  FILE* output = fopen("../output/LICENSE.packingtape", "w");
  CompressorPredictor* p = malloc(sizeof(*p));
  CP_New(p, NULL);
  compress(input, output, p);

  FILE* ninput = fopen("../output/LICENSE.packingtape", "rb");
  FILE* noutput = fopen("../output/LICENSE.original", "w");
  DecompressorPredictor* pnew = malloc(sizeof(*pnew));
  *pnew = (DecompressorPredictor) {};
  DP_New(pnew);
  decompress(ninput, noutput, pnew);
}

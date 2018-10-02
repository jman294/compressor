#include <stdlib.h>

#include "packingtape/compressor.h"
#include "packingtape/compressorpredictor.h"
#include "packingtape/decompressor.h"
#include "packingtape/decompressorpredictor.h"

int main () {
  FILE* input = fopen("../LICENSE", "rb");
  FILE* output = fopen("../output/LICENSE.packingtape", "w");
  CompressorPredictor* p = malloc(sizeof(*p));
  CP_New(p);
  compress(input, output, p);

  /*FILE* ninput = fopen("/home/jack/Git/compressor/output/LICENSE.packingtape", "rb");*/
  /*FILE* noutput = fopen("/home/jack/Git/compressor/output/LICENSE.original", "w");*/
  FILE* ninput = fopen("../output/LICENSE.packingtape", "rb");
  FILE* noutput = fopen("../output/LICENSE.original", "w");
  DecompressorPredictor* pnew = malloc(sizeof(*pnew));
  *pnew = (DecompressorPredictor) {};
  DP_New(pnew);
  decompress(ninput, noutput, pnew);
}

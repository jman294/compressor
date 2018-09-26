#include <stdlib.h>
#include "packingtape/decompressor.h"
#include "packingtape/decompressorpredictor.h"
#include "packingtape/compressor.h"
#include "packingtape/compressorpredictor.h"

int main () {
  FILE* input = fopen("/home/jack/Git/compressor/LICENSE", "rb");
  FILE* output = fopen("/home/jack/Git/compressor/output/LICENSE.packingtape", "w");
  CompressorPredictor* p = malloc(sizeof(*p));
  *p = (CompressorPredictor) {};
  compress(input, output, p);

  input = fopen("/home/jack/Git/compressor/output/LICENSE.packingtape", "rb");
  output = fopen("/home/jack/Git/compressor/output/LICENSE.original", "w");
  DecompressorPredictor* pnew = malloc(sizeof(*pnew));
  *pnew = (DecompressorPredictor) {};
  decompress(input, output, pnew);
}

#include <stdlib.h>
#include "packingtape/compressor.h"
#include "packingtape/compressorpredictor.h"

int main () {
  FILE* input = fopen("/home/jack/Git/compressor/LICENSE", "rb");
  FILE* output = fopen("/home/jack/Git/compressor/output/LICENSE.packingtape", "w");
  CompressorPredictor* p = malloc(sizeof(*p));
  *p = (CompressorPredictor) {};
  compress(input, output, p);
}

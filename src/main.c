#include <stdlib.h>

#include "packingtape/compressor.h"
#include "packingtape/compressorpredictor.h"
#include "packingtape/decompressor.h"
#include "packingtape/decompressorpredictor.h"

int main (int argc, char ** argv) {
  // Chech arguments: packingtape c/d input output
  if (argc!=4 || (argv[1][0]!='c' && argv[1][0]!='d')) {
    printf("To compress:   packingtape c input output\n"
           "To decompress: packingtape d input output\n");
    exit(1);
  }

  // Open files
  FILE *input=fopen(argv[2], "rb");
  if (!input) perror(argv[2]), exit(1);
  FILE *output=fopen(argv[3], "wb");
  if (!output) perror(argv[3]), exit(1);

  if (argv[1][0] == 'c') {
    CompressorPredictor* p = malloc(sizeof(*p));
    *p = (CompressorPredictor) {};
    compress(input, output, p);
  } else if (argv[1][0] == 'd') {
    DecompressorPredictor* p = malloc(sizeof(*p));
    *p = (DecompressorPredictor) {};
    decompress(input, output, p);
  }
}

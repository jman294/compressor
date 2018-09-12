#include "compressorpredictor.h"
#include <stdio.h>

#ifndef COMPRESSOR_H_   /* Include guard */
#define COMPRESSOR_H_

void compress(FILE* input, FILE* output, CompressorPredictor* p);

#endif // COMPRESSOR_H_

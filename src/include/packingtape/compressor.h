#ifndef COMPRESSOR_H_   /* Include guard */
#define COMPRESSOR_H_

#include <stdio.h>

#include "compressorpredictor.h"

void compress(FILE* input, FILE* output, CompressorPredictor* p);

#endif // COMPRESSOR_H_

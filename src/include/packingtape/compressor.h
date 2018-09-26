#ifndef COMPRESSOR_H_   /* Include guard */
#define COMPRESSOR_H_

#include <stdio.h>

#include "compressorpredictor.h"

typedef unsigned long uint32;

void compress(FILE* input, FILE* output, CompressorPredictor* p);

#endif // COMPRESSOR_H_

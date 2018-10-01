#ifndef DECOMPRESSOR_H_   /* Include guard */
#define DECOMPRESSOR_H_

#include <stdio.h>

#include "decompressorpredictor.h"

void decompress(FILE* input, FILE* output, DecompressorPredictor* p);

#endif // DECOMPRESSOR_H_

#ifndef UTIL_H_   /* Include guard */
#define UTIL_H_

#include <stdio.h>

typedef unsigned long uint32;

int prediction (int cxt, int ct[512][2]);

void flush (uint32* x1, uint32* x2, FILE* archive);

#endif // UTIL_H_

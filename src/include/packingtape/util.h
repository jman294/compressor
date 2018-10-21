#ifndef UTIL_H_   /* Include guard */
#define UTIL_H_

#include <stdio.h>
#include <stdint.h>

typedef uint32_t uint32;

// 32 Bit Context
typedef uint32_t context;

void flush (uint32* x1, uint32* x2, FILE* archive);

#endif // UTIL_H_

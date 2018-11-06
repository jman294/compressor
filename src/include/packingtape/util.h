#ifndef UTIL_H_   /* Include guard */
#define UTIL_H_

#include <stdio.h>
#include <stdint.h>

// 32 Bit Context
typedef uint16_t context;

void flush (uint32_t* x1, uint32_t* x2, FILE* archive);

#endif // UTIL_H_

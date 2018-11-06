#include "util.h"

int prediction (int cxt, int ct[512][2]) {
  return 4096*(ct[cxt][1]+1)/(ct[cxt][0]+ct[cxt][1]+2);
}

void flush (uint32_t* x1, uint32_t* x2, FILE* archive) {
  while (((*x1^*x2)&0xff000000)==0) {
    putc(*x2>>24, archive);
    *x1<<=8;
    *x2=(*x2<<8)+255;
  }
  putc(*x2>>24, archive);  // First unequal byte
}

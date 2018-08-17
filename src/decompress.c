#include <stdio.h>
#include <string.h>
#include <assert.h>

// BUILT FROM FPAQ0, GPL, Matt Mahoney

typedef unsigned long U32;  // 32 bit type

int prediction (int cxt, int ct[512][2]) {
  return 4096*(ct[cxt][1]+1)/(ct[cxt][0]+ct[cxt][1]+2);
}

int decode (U32* x1, U32* x2, U32* x, int prediction, FILE* archive) {

  // Update the range
  const U32 xmid = (*x1) + (((*x2)-(*x1)) >> 12) * prediction;
  assert(xmid >= (*x1) && xmid < (*x2));
  int y=0;
  if ((*x)<=xmid) {
    y=1;
    (*x2)=xmid;
  }
  else
    (*x1)=xmid+1;

  // Shift equal MSB's out
  while ((((*x1)^(*x2))&0xff000000)==0) {
    (*x1)<<=8;
    (*x2)=((*x2)<<8)+255;
    int c=getc(archive);
    if (c==EOF) c=0;
    (*x)=((*x)<<8)+c;
  }
  return y;
}

void flush (U32* x1, U32* x2, FILE* archive) {
  while (((*x1^*x2)&0xff000000)==0) {
    putc(*x2>>24, archive);
    *x1<<=8;
    *x2=(*x2<<8)+255;
  }
  putc(*x2>>24, archive);  // First unequal byte
}

int main () {
  U32 x1 = 0;
  U32 x2 = 0xffffffff;
  U32 x = 0;

  int cxt;  // Context: last 0-8 bits with a leading 1
  int ct[512][2];  // 0 and 1 counts in context cxt
  memset(ct, 0, sizeof(ct));

  FILE *in=fopen("fpaq0.cpp.dupzip", "rb");
  FILE *out=fopen("fpaq0.cpp.original", "wb");

  for (int i=0; i<4; ++i) {
    int c=getc(in);
    if (c==EOF) c=0;
    x=(x<<8)+(c&0xff);
  }
  printf("%ld", x);

  while (!decode(&x1, &x2, &x, prediction(cxt, ct), in)) {
    int c=1;
    while (c<256)
      c+=c+decode(&x1, &x2, &x, prediction(cxt, ct), in);
    putc(c-256, out);
  }

  fclose(in);
  fclose(out);
}

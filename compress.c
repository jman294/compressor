#include <stdio.h>
#include <assert.h>

// BUILT FROM FPAQ0, GPL, Matt Mahoney

typedef unsigned long U32;  // 32 bit type

int prediction (int cxt, int ct[512][2]) {
  return 4096*(ct[cxt][1]+1)/(ct[cxt][0]+ct[cxt][1]+2);
}

void encode (U32* x1, U32* x2, int y, FILE* archive, int prediction) {
  // Update the range
  const U32 xmid = *x1 + ((*x2-*x1) >> 12) * prediction;
  assert(xmid >= *x1 && xmid < *x2);
  if (y)
    *x2=xmid;
  else
    *x1=xmid+1;

  // Shift equal MSB's out
  while (((*x1^*x2)&0xff000000)==0) {
    putc(*x2>>24, archive);
    *x1<<=8;
    *x2=(*x2<<8)+255;
  }
}

int decode (U32* x1, U32* x2, int* x, FILE* archive, int prediction) {
  // Update the range
  const U32 xmid = *x1 + ((*x2-*x1) >> 12) * prediction;
  assert(xmid >= *x1 && xmid < *x2);
  int y=0;
  if (*x<=xmid) {
    y=1;
    *x2=xmid;
  }
  else
    *x1=xmid+1;

  // Shift equal MSB's out
  while (((*x1^*x2)&0xff000000)==0) {
    *x1<<=8;
    *x2=(*x2<<8)+255;
    int c=getc(archive);
    if (c==EOF) c=0;
    *x=(*x<<8)+c;
  }
  /*printf("%d", y);*/
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

  int cxt;  // Context: last 0-8 bits with a leading 1
  int ct[512][2];  // 0 and 1 counts in context cxt

  FILE *archive=fopen("fpaq0.cpp.dupzip", "w+b");
  FILE *in=fopen("fpaq0.cpp", "rb");
  FILE *out=fopen("fpaq0.cpp.original", "w+b");

  int c;

  while ((c=getc(in))!=EOF) {
    encode(&x1, &x2, 0, archive, prediction(cxt, ct));
    for (int i=7; i>=0; --i)
        encode(&x1, &x2, (c>>i)&1, archive, prediction(cxt, ct));
  }
  encode(&x1, &x2, 1, archive, prediction(cxt, ct));  // EOF code
  flush(&x1, &x2, archive);

  int x;
  x1 = 0;
  x2 = 0xffffffff;

  for (int i=0; i<4; ++i) {
    int c=getc(archive);
    if (c==EOF) c=0;
    x=(x<<8)+(c&0xff);
  }

  while (!decode(&x1, &x2, &x, archive, prediction(cxt, ct))) {
    printf("%lu %lu\n", x1, x2);
    int c=1;
    while (c<256)
      c+=c+decode(&x1, &x2, &x, archive, prediction(cxt, ct));
    putc(c-256, out);
  }

  fclose(in);
  fclose(out);
  fclose(archive);
}

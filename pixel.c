#include "common.h"

void divide_image(image *a, image *b) {
  int h= a->height;
  int w= a->width;
  int i, z;
  gray *pa, *pb;
  if (b->height != h || b->width != w) error("divide_image: size mismatch.");
  for (z= 1; z < 4; z++) {
    pa= a->chan[z]; pb= b->chan[z];
    if (!pa || !pb) continue;
    for (i= 0; i < w * h; i++) {
      *pa= (real)*pa / *pb;
      pa++; pb++;
    }
  }
}

void contrast_image(image *im, real black, real white) {
  gray *p;
  real m, q;
  int z;
  unsigned long int i, l= im->width * im->height;
  if (white == black) {
    for (z= 1; z < 4; z++) {
      p= im->chan[z];
      if (!p) continue;
      for (i=0; i<l; i++,p++) {
        if (*p <= black) *p= 0;
        else *p= 1;
      }
    }
    return;
  }
  //mw+q=M mb+q=-M m(w-b)=2M
  m= 1.0 / (white - black) ;
  q= -m * black;

  if (black < white) {
    for (z= 1; z < 4; z++) {
      p= im->chan[z];
      if (!p) continue;
      for (i=0; i<l; i++,p++) {
        if (*p <= black) *p= 0;
        else if (*p >= white) *p= 1;
        else *p= *p * m + q;
      }
    }
    return;
  }

  else { // white < black
    for (z= 1; z < 4; z++) {
      p= im->chan[z];
      if (!p) continue;
      for (i=0; i<l; i++,p++) {
        if (*p >= black) *p= 0;
        else if (*p <= white) *p= 1;
        else *p= *p * m + q;
      }
    }
    return;
  }
}

void diff_image(image *a, image *b) {
  int h= a->height;
  int w= a->width;
  int i, z;
  gray *pa, *pb;
  if (b->height != h || b->width != w) error("diff_image: size mismatch.");
  for (z= 1; z < 4; z++) {
    pa= a->chan[z]; pb= b->chan[z];
    if (!pa || !pb) continue;
    for (i= 0; i < w * h; i++) {
      *pa= *pa - *pb + 128;
      pa++; pb++;
    }
  }
}

void patch_image(image *a, image *b) {
  int h= a->height;
  int w= a->width;
  int i, z;
  gray *pa, *pb;
  if (b->height != h || b->width != w) error("patch_image: size mismatch.");
  for (z= 1; z < 4; z++) {
    pa= a->chan[z]; pb= b->chan[z];
    if (!pa || !pb) continue;
    for (i= 0; i < w * h; i++) {
      *pa= *pa + *pb - 128;
      pa++; pb++;
    }
  }
}

// vim: sw=2 ts=2 sts=2 expandtab:

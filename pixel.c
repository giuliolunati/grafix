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

void yuv_from_rgb_image(image *im) {
  if (image_depth(im) < 3) return;
  int z;
  for (z= 1; z < 4; z++) if (! im->chan[z]) error("ybr_from_rgb_image: missing channel.");
  gray *r= im->chan[1];
  gray *g= im->chan[2];
  gray *b= im->chan[3];
  gray y,u,v;
  unsigned long int j, l= im->width * im->height;
  for (j= 0; j< l; j++) {
    y=
      +0.29900* *r +0.5870* *g +0.1140* *b;
    u=
      -0.168736* *r -0.331264* *g +0.5* *b;
    v=
      +0.5* *r -0.418688* *g -0.081312* *b;
    *r= y; *g= u; *b= v;
    r++; g++; b++;
  }
}

void rgb_from_yuv_image(image *im) {
  if (image_depth(im) < 3) return;
  int z;
  for (z= 1; z < 4; z++) if (! im->chan[z]) error("ybr_from_rgb_image: missing channel.");
  gray *y= im->chan[1];
  gray *u= im->chan[2];
  gray *v= im->chan[3];
  gray r,g,b;
  unsigned long int j, l= im->width * im->height;
  for (j= 0; j< l; j++) {
    r= *y /* +0.000* *u */ +1.402* *v;
    g= *y -0.344136* *u -0.714136* *v;
    b= *y +1.772* *u /* +0.000* *v */;
    *y= r; *u= g; *v= b;
    y++; u++; v++;
  }
}

image *image_luma(image *im) {
  if (image_depth(im) < 3) error("image_luma: yet grayscale");
  image *om= image_clone(im,1,0,0);
  int z;
  for (z= 1; z < 4; z++) if (! im->chan[z]) error("ybr_from_rgb_image: missing channel.");
  gray *r= im->chan[1];
  gray *g= im->chan[2];
  gray *b= im->chan[3];
  gray *l= om->chan[1];
  unsigned long int j, len= im->width * im->height;
  for (j= 0; j<len; j++) {
    *l= 0.299* *r +0.587* *g +0.114* *b;
    r++; g++; b++; l++;
  }
  return om;
}



// vim: sw=2 ts=2 sts=2 expandtab:

#include "common.h"

image *image_maxmin(image *im, float dx, float dy) {
  float cx, cy;
  if (dx!=0) cx= exp(-0.333/fabs(dx));
  if (dy!=0) cy= exp(-0.333/fabs(dy));
  int x, y, z, h= im->height, w= im->width;
  image *om= image_clone(im, 0, 0, 0);
  om->ex= im->ex;
  gray t, *v0, *v1;
  v0= malloc(w * sizeof(*v0));
  v1= malloc(w * sizeof(*v1));
  gray *pi;
  gray *po;
  for (z= 1; z < 4; z++) {
    pi= im->chan[z];
    if (! pi) continue;
    po= om->chan[z];
    for (y= 0; y < h; y++) {
      for (x= 0; x < w; x++) {
        v1[x]= *(pi++);
      }
      if (dx>0) { // max
        for (x= 1; x < w; x++) {
          t= v1[x - 1] * cx;
          if (v1[x] < t) v1[x]= t;
        }
        for (x= w - 2; x >= 0; x--) {
          t= v1[x + 1] * cx;
          if (v1[x] < t) v1[x]= t;
        }
      }
      else
      if (dx<0) { // min
        for (x= 1; x < w; x++) {
          t= (1 - v1[x-1]) * cx;
          v1[x]= fmin(v1[x], 1-t);
        }
        for (x= w - 2; x >= 0; x--) {
          t= (1 - v1[x+1]) * cx;
          v1[x]= fmin(v1[x], 1-t);
        }
      }
      if (dy > 0) {
        for (x= 0; x < w; x++) {
          t= v0[x] * cy;
          if (v1[x] < t) v1[x]= t;
        }
      }
      else
      if (dy < 0) {
        for (x= 0; x < w; x++) {
          t= (1 - v0[x]) * cy;
          v1[x]= fmin(v1[x], 1-t);
        }
      }
      for (x= 0; x < w; x++) {
        *(po++)= v1[x];
      }
      memcpy(v0, v1, w * sizeof(*v0));
    }
    for (y= 1; y < h; y++) {
      po -= w;
      for (x= w - 1; x >= 0; x--) v1[x]= *(--po);
      if (dy>0) {
        for (x= 0; x < w; x++) {
          t= v0[x] * cy;
          if (v1[x] < t) v1[x]= t;
        }
      }
      else
      if (dy<0) {
        for (x= 0; x < w; x++) {
          t= (1 - v0[x]) * cy;
          v1[x]= fmin(v1[x], 1-t);
        }
      }
      for (x= 0; x < w; x++, po++) {
        *(po)= v1[x];
      }
      memcpy(v0, v1, w * sizeof(*v0));
    }
  }
  free(v0); free(v1);
  return om;
}

vector *histogram_of_image(image *im, int chan) {
  vector *hi= make_vector(256);
  hi->len= hi->size;
  gray v, *p= im->chan[chan];
  real *d= hi->data;
  int x, y, h= im->height, w= im->width;
  for (y= 0; y < h; y++) {
    for (x= 0; x < w; x++) {
      v= *p;
      if (v < 0) d[0] += 1;
      else
      if (v > 1) d[255] += 1;
      else
      d[(int)round(v * 255)] += 1;
      p++;
    }
  }
  return hi;
}

void mean_y(image *im, uint d) {
  uint w= im->width;
  uint h= im->height;
  real *v= calloc(w * (d + 1), sizeof(*v));
  real *r1, *r, *rd;
  int y, i;
  gray *p, *q, *end;
  for (y= 0; y < h; y++) {
    i= (y+1) % (d+1);
    rd= v + w * i;
    i= (i+d) % (d+1);
    r= v + w * i;
    i= (i+d) % (d+1);
    r1= v + w * i;
    p= im->chan[1] + y * w;
    i= y - d/2;
    if (y >= d) q= im->chan[1] + i * w;
    else q= 0;
    end= p + w;
    for (; p < end; p++, r1++, r++, rd++) {
      *r= *r1 + *p;
      if (q) *(q++)= (*r - *rd) / d;
    }
  }
  free(v);
}

void darker_image(image *a, image *b) {
  int h= a->height;
  int w= a->width;
  int i,z;
  gray *pa, *pb;
  if (b->height != h || b->width != w) error("darker_image: size mismatch.");
  for (z=1; z < 4; z++) {
    pa= a->chan[z]; pb= b->chan[z];
    if (!pa || !pb) continue;
    for (i= 0; i < w * h; i++) {
      if (*pa > *pb) { *pa= *pb; };
      pa++; pb++;
    }
  }
}

void lighter_image(image *a, image *b) {
  int h= a->height;
  int w= a->width;
  int i,z;
  gray *pa, *pb;
  if (b->height != h || b->width != w) error("lighter_image: size mismatch.");
  for (z=1; z < 4; z++) {
    pa= a->chan[z]; pb= b->chan[z];
    if (!pa || !pb) continue;
    for (i= 0; i < w * h; i++) {
      if (*pa < *pb) { *pa= *pb; };
      pa++; pb++;
    }
  }
}

void calc_statistics(image *im, int verbose) {
  // threshold histogram
  vector *thr= make_vector(256);
  thr->len= thr->size;
  real *pt= thr->data;
  // border histogram
  vector *hb= make_vector(256);
  hb->len= hb->size;
  real *pb= hb->data;
  // area histogram
  vector *ha= make_vector(256);
  ha->len= ha->size;
  real *pa= ha->data;
  real area, border, thickness, black, graythr, white;
  int i, x, y, t;
  int h= im->height, w= im->width;
  gray *pi= im->chan[1];
  gray *px= pi + 1;
  gray *py= pi + w;
  short a, b, c;
  uint d;
  // histograms
  for (y= 0; y < h; y++) {
    for (x= 0; x < w; x++) {
      a= *pi;
      b= *px;
      pa[a]++;
      if ((x >= w - 1) || (y >= h - 1)) {continue;} 
      if (a > b) {c= b; b= a; a= c;}
      pb[a]++; pb[b]--;
      d= b - a; d *= d;
      pt[a] += d; pt[b] -= d;
      a= *pi;
      b= *py;
      if (a > b) {c= b; b= a; a= c;}
      pb[a]++; pb[b]--;
      d= b - a; d *= d;
      pt[a] += d; pt[b] -= d;
      pi++; px++, py++;
    }
    pi++; px++, py++;
  }
  // gray threshold
  cumul_vector(thr);
  cumul_vector(hb);
  // for (i= 0; i < 256; i++) {thr->data[i] /= sqrt(hb->data[i] + 4);}
  t= index_of_max(thr);
  graythr= t / 255.0;
  // border, area, thickness, nchars
  border= hb->data[t] * 0.8;
  cumul_vector(ha);
  area= ha->data[t];
  thickness= 2 * area / border;
  // black
  black= 0;
  for (i= 0; i < t; i++) {
    black += ha->data[i];
  }
  black= (t - (black / area)) / 255.0;
  // white
  white= 255.0 * w * h - (area * t);
  for (i= t + 1; i < 256; i++) {
    white -= ha->data[i];
  }
  white /= (w * h - area) * 255.0;
  if (verbose) {printf(
      "black: %g gray: %g white: %g thickness: %g area: %g \n",
      black, graythr, white, thickness, area 
  );}
  im->black= black;
  im->graythr= graythr;
  im->white= white;
  im->thickness= thickness;
  im->area= area;
}


// vim: sw=2 ts=2 sts=2 expandtab:

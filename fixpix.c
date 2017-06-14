#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#define uchar unsigned char
#define uint unsigned int
#define ulong unsigned long int
#define real float
#define MAXVAL 6552 
#define MAXSHORT 32767
#define EQ(a, b) (0 == strcmp((a), (b)))
#define IS_IMAGE(p) (((char *)p)->type == 'I')
#define IS_HISTOGRAM(p) (((char *)p)->type == 'H')

//// ERRORS ////

void error(const char *msg) {
  fprintf(stderr, "ERROR: ");
  fprintf(stderr, "%s", msg);
  fprintf(stderr, "\n");
  exit(1);
}

//// SRGB ////
  // 0 <= srgb <= 1  0 <= lin <= 1
  // srgb < 0.04045: lin = srgb / 12.92
  // srgb > 0.04045: lin = [(srgb + 0.055) / 1.055]^2.4

short lin_from_srgb[256];
uchar srgb_from_lin[MAXVAL + 1];

void init_srgb() {
  int i, l0, s;
  real l;
  for (s = 0; s <= 255; s++) {
    l = (s + 0.5) / 255.5;
    if (l < 0.04045) l /= 12.92;
    else {
      l = (l + 0.055) / 1.055;
      l = pow(l, 2.4);
    }
    l = roundf(l * MAXVAL);
    lin_from_srgb[s] = l;
    if (s == 0) { l0 = 0; continue; }
    for (i = l0; i <= (l0 + l) / 2; i++) {
      srgb_from_lin[i] = s - 1;
    }
    for (; i <= l; i++) {
      srgb_from_lin[i] = s;
    }
    l0 = l;
  }
}

//// HISTOGRAMS ////

typedef struct { // histogram
  real *data;
  uint len;
  uint size;
  real x0;
  real dx;
  char type;
} histogram;

histogram *make_histogram(uint size) {
  histogram *hi;;
  if (! (hi = malloc(sizeof(histogram))))
    error("Can't alloc histogram.");
  hi->size = size;
  if ( ! (
    hi->data = malloc(size * sizeof(hi->data))
  )) error("Can't alloc histogram data.");
  hi->len = 0;
  hi->x0 = 0.0;
  hi->dx = 1.0;
  hi->type = 'H';
  return hi;
}

void destroy_histogram(histogram *h) {
  if (! h) return;
  if (h->data) free(h->data);
  free(h);
}

void cumul_histogram(histogram *hi) {
  uint i;
  real *p = hi->data;
  for (i = 1; i < hi->len; i ++) {
    p[i] += p[i - 1];
  }
}

void diff_histogram(histogram *hi) {
  uint i;
  real *p = hi->data;
  for (i = hi->len - 1; i > 0; i --) {
    p[i] -= p[i - 1];
  }
}

void dump_histogram(FILE *f, histogram *hi) {
  uint i;
  real *p = hi->data;
  fprintf(f, "# x0: %f dx: %f len: %d\n",
      hi->x0, hi->dx, hi->len
  );
  for (i = 0; i < hi->len; i ++) {
    fprintf(f, "%f\n", p[i]);
  }
}

//// IMAGES ////

typedef struct { // image
  short *data;
  uint width;
  uint height;
  real lpp; // lines-per-page: height / line-height
  char type;
} image;

real default_lpp = 40;

image *make_image(int width, int height) {
  image *im;
  short *data;
  if (height < 1 || width < 1) return NULL;
  data = calloc(width * height, sizeof(short));
  if (! data) return NULL;
  im = malloc(sizeof(image));
  if (! im) return NULL;
  im->height = height;
  im->width = width;
  im->data = data;
  im->lpp = 0;
  im->type = 'I';
  return im;
}

void destroy_image(image *im) {
  if (! im) return;
  if (im->data) free(im->data);
  free(im);
}

image *read_image(FILE *file, int layer) {
  int x, y, type, depth, height, width, binary;
  image *im;
  uchar *buf, *ps;
  short *pt;
  if (! file) error("File not found.");
  if (4 > fscanf(file, "P%d %d %d %d\n", &depth, &width, &height, &type)) {
    error("Not a PNM file.");
  }
  switch (depth) {
    case 5: depth = 1; break;
    case 6: depth = 3; break;
    default: error("Invalid depth.");
  }
  switch (type) {
    case 255: break;
    default: error("Invalid bits.");
  }
  if (width < 1 || height < 1) error("Invalid dimensions.");
  im = make_image(width, height);
  if (!im) error("Cannot make image.");;
  buf = malloc(width * depth);
  pt = im->data;
  for (y = 0; y < height; y++) {
    if (width > fread(buf, depth, width, file))  error("Unexpected EOF");
    ps = buf + layer;
    for (x = 0; x < width; x++) {
      *(pt++) = *(ps += depth);
    }
  }
  free(buf);
  return im;
}

void *image_from_srgb(image *im) {
  short *p, *end;
  short i;
  end = im->data + (im->height * im->width);
  for (p = im->data; p < end; p++) {
    i = *p;
    if (i > 255) *p = MAXVAL;
    else
    if (i < 0) *p = 0;
    else
    *p = lin_from_srgb[*p];
  }
}

int write_image(image *im, FILE *file) {
  int x, y;
  uchar *buf, *pt;
  short i, *ps;
  assert(file);
  fprintf(file, "P5\n%d %d\n255\n", im->width, im->height);
  buf = malloc(im->width * sizeof(*buf));
  assert(buf);
  ps = im->data;
  assert(ps);
  for (y = 0; y < im->height; y++) {
    pt = buf;
    for (x = 0; x < im->width; x++) {
      i = *(ps++);
      if (i < 1) {*(pt++) = 0; continue;}
      else
      if (i > MAXVAL) {*(pt++) = 255; continue;}
      else
      *(pt++) = srgb_from_lin[i];
    }
    if (im->width > fwrite(buf, 1, im->width, file)) error("Error writing file.");
  }
  free(buf);
}

image *rotate_image(image *im, float angle) {
  int w = im->width, h = im->height;
  int x, y, dx, dy;
  image *om = make_image(h, w);
  om->lpp = im->lpp;
  short *i, *o;
  i = im->data; o = om->data;
  switch ((int)angle) {
    case 90:
      o += h - 1; dx = h; dy = -1;
      break;
    case 180:
      om->width = w; om->height = h;
      o += h * w - 1; dx = -1; dy = -w;
      break;
    case 270:
    case -90:
      o += h * w - h; dx = -h; dy = 1;
      break;
    default: error("rotate: only +/-90, 180, 270");
  }
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
      *o = *i; i++; o += dx;
    }
    o += dy - w * dx;
  }
  return om;
}

image *image_background(image *im) {
  // find light background
  real d = im->lpp;
  if (d <= 0) d = default_lpp;
  d /= im->height;
  d = exp(-d);
  int x, y, h = im->height, w = im->width;
  image *om = make_image(w, h);
  om->lpp = im->lpp;
  real t, *v0, *v1;
  v0 = malloc(w * sizeof(*v0));
  v1 = malloc(w * sizeof(*v1));
  short *pi = im->data;
  short *po = om->data;
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) v1[x] = *(pi++);
    for (x = 1; x < w; x++) {
      t = v1[x - 1] * d;
      if (v1[x] < t) v1[x] = t;
    }
    for (x = w - 2; x >= 0; x--) {
      t = v1[x + 1] * d;
      if (v1[x] < t) v1[x] = t;
    }
    if (y > 0) for (x = 0; x < w; x++) {
      t = v0[x] * d;
      if (v1[x] < t) v1[x] = t;
    }
    for (x = 0; x < w; x++) {
      *(po++) = round(v1[x]);
    }
    memcpy(v0, v1, w * sizeof(*v0));
  }
  assert(pi == im->data + w * h);
  assert(po == om->data + w * h);
  for (y = 1; y < h; y++) {
    po -= w;
    for (x = w - 1; x >= 0; x--) v1[x] = *(--po);
    for (x = 0; x < w; x++) {
      t = v0[x] * d;
      if (v1[x] < t) v1[x] = t;
    }
    for (x = 0; x < w; x++) {
      *(po++) = round(v1[x]);
    }
    memcpy(v0, v1, w * sizeof(*v0));
  }
  assert(po == om->data + w);
  free(v0); free(v1);
  return om;
}

void *divide_image(image *a, image *b) {
  int h = a->height;
  int w = a->width;
  int i;
  short *pa, *pb;
  if (b->height != h || b->width != w) error("Dimensions mismatch.");
  pa = a->data; pb = b->data;
  for (i = 0; i < w * h; i++) {
    *pa = (real)*pa / *pb * MAXVAL;
    pa++; pb++;
  }
}

histogram *histogram_of_image(image *im, int a, int z) {
  histogram *hi = make_histogram(z - a + 1);
  hi->x0 = a;
  hi->len = hi->size;
  short *p = im->data;
  real *d = hi->data;
  int x, y, h = im->height, w = im->width;
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
      if (*p > z) d[z - a] += 1;
      else if (*p < a) d[0] += 1;
      else d[*p - a] += 1;
      p ++;
    }
  }
  return hi;
}

//// STACK ////
#define STACK_SIZE 256
void *stack[STACK_SIZE];
#define SP stack[sp]
#define SP_1 stack[sp - 1]
#define SP_2 stack[sp - 2]

int sp = 0;
void *swap() {
  void *p;
  if (sp < 2) error("Stack underflow");
  p = SP_2; SP_2 = SP_1; SP_1 = p;
}
void *pop() {
  if (sp < 1) error("Stack underflow");
  sp--;
  return SP;
}
void push(void *p) {
  if (sp >= STACK_SIZE) error("Stack overflow");
  if (SP) free(SP);
  SP = p;
  sp++;
}

void help_exit(char **args) {
  printf("\nUSAGE: %s COMMANDS...\n\n", *args);
  printf("COMMANDS:\n\
  -h, --help: this help\n\
  FILENAME:   load a PNM image\n\
  -:          load from STDIN\n\
  bg FLOAT:   \n\
  div:        \n\
  fix-bg:     \n\
  histo:      \n\
  lpp FLOAT:  \n\
  quit:       \n\
  rot ANGLE:  rotate of ANGLE (only 90, -90, 180, 270)\n\
  \n");
  exit(0);
}

//// MAIN ////
#define ARG_IS(x) EQ((x), *arg)
int main(int argc, char **args) {
  int i, c, d;
  init_srgb();
  char **arg = args;

  image * im;
  if (argc < 2) help_exit(args);
  while (*(++arg)) { // -
    if (ARG_IS("-")) {
      push(read_image(stdin, 0));
      image_from_srgb((image*)SP_1);
    }
    else // -h, --help
    if (ARG_IS("-h") || ARG_IS("--help")) {
      help_exit(args);
    }
    else // bg FLOAT
    if (ARG_IS("bg")) {
      push(image_background((image*)SP_1));
    }
    else // div
    if (ARG_IS("div")) {
      divide_image((image*)SP_2, (image*)SP_1);
      pop();
    }
    else // fix-bg
    if (ARG_IS("fix-bg")) {
      push(image_background((image*)SP_1));
      divide_image((image*)SP_2, (image*)SP_1);
      pop();
    }
    else // histo
    if (ARG_IS("histo")) {
      histogram *hi = histogram_of_image((image*)SP_1, 0, MAXVAL);
      cumul_histogram(hi);
      dump_histogram(stdout, hi);
    }
    else // lpp FLOAT
    if (ARG_IS("lpp")) {
      if (! *(++arg)) break;
      default_lpp = atof(*arg);
      if (sp) ((image*)SP_1)->lpp = default_lpp;
    }
    else // quit
    if (ARG_IS("quit")) exit(0);
    else // rot +-90, 180, 270
    if (ARG_IS("rot")) {
      if (! *(++arg)) break;
      push(rotate_image((image*)SP_1, atof(*arg)));
      swap(); pop();
    }
    else { // STRING
      push(read_image(fopen(*arg, "rb"), 0));
      image_from_srgb((image*)SP_1);
    }
  }
  if (sp > 0) write_image(pop(), stdout);
}

// vim: sw=2 ts=2 sts=2 expandtab:

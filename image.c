#include "common.h"
#include <errno.h>

#define EQ(a, b) (0 == strcmp((a), (b)))

//// IMAGES ////

real default_ex= 25;

int image_depth(image *im) {
  int depth;
  if (! im->chan[1]) error("image_depth: missing chan[1]");
  if (im->chan[2]) {
    if (! im->chan[3]) error("image_depth: missing chan[3]");
    depth= 3;
  } else {
    if (im->chan[3]) error("image_depth: missing chan[2]");
    depth= 1;
  }
  if (im->chan[0]) depth ++;
  return depth;
}

image *image_make(int depth, int width, int height) {
  assert(1 <= depth && depth <= 4);
  if (height < 1 || width < 1) error("image_make: size <= 0");
  int i;
  int opaque= depth % 2;
  gray *p;
  image *im= malloc(sizeof(image));
  if (! im) error("image_make: can't alloc memory");
  for (i= opaque; i < depth+opaque; i ++) {
    p= calloc (width * height, sizeof(*p));
    if (! p) error("image_make: can't alloc memory");
    im->chan[i]= p;
  } for (; i < 4; i ++) im->chan[i]= NULL;
  if (opaque) im->ALPHA= NULL;
  im->SEL= NULL;
  im->magic= 'I';
  im->width= width;
  im->height= height;
  im->ex= default_ex;
  im->pag= 0;
  im->black= im->graythr= im->white= -1;
  im->area= im->thickness= -1;
  return im;
}

void destroy_image(image *im) {
  if (! im) return;
  int i;
  for (i= 0; i < 4; i ++) {
    if (im->chan[i]) free(im->chan[i]);
  }
  if (im->SEL) free(im->SEL);
  free(im);
}

void add_channel(image *im, int z) {
  if (im->chan[z]) return;
  gray *p= malloc(sizeof(gray) * im->width * im->height);
  if (! p) error("add_channel: out of memory");
  im->chan[z]= p;
}

image *image_clone(image *im, int depth, int width, int height) {
  if (depth < 1) depth= image_depth(im);
  if (width < 1) width= im->width;
  if (height < 1) height= im->height;
  int z;
  gray *p;
  if (depth > 4) error("clone_image: invalid depth");
  image *om= malloc(sizeof(*om));
  if (! om) error("clone_image: out of memory");
  memcpy(om, im, sizeof(*om));
  om->width= width;
  om->height= height;
  uint len= width * height * sizeof(gray);
  for (z= 0; z < 5; z++) om->chan[z]= NULL;
  for (z= depth%2; z < depth+depth%2; z++) add_channel(om, z);
  return om;
}

image *image_copy(image *im) {
  int z;
  gray *pi;
  image *om= image_clone(im, 0, 0, 0);
  uint len= im->width * im->height * sizeof(gray);
  for (z=0; z<5; z++) {
    pi= im->chan[z];
    if (! pi) continue;
    memcpy(om->chan[z], pi, len);
  }
  return om;
}

char *read_next_token(FILE *file) {
  char c;
  int i;
  char *buf= malloc(16);
  unsigned long int p0, p1;
  while (1) {
    i= fscanf(file, " ");
    i= fscanf(file, "#%c", &c);
    if (i > 0) {
      while (c != '\n') i= fscanf(file, "%c", &c);
      continue;
    }
    break;
  }
  if ( ! fscanf(file, "%15s", buf) ) return "";
  return buf;
}

image *image_read_pnm(FILE *file) {
  int x, y, prec, magic, depth, height, width, binary;
  image *im;
  uchar *buf, *ps;
  int z, i;
  char c, *tok;

  assert(file);
  if (1 > fscanf(file, "P%d ", &magic))
  { error("image_read_pnm: wrong magic."); }
  if (magic < 7) { // PNM
    if (0 >= (width= atoi(read_next_token(file))))
    { error("image_read_pnm: wrong width."); }
    if (0 >= (height= atoi(read_next_token(file))))
    { error("image_read_pnm: wrong height."); }
    if (0 >= (prec= atoi(read_next_token(file))))
    { error("image_read_pnm: wrong precision"); }
    if (1 > fscanf(file, "%c", &c) || 
      (c != ' ' && c != '\t' && c != '\n')
    ) {
      error("image_read_pnm: no w/space after precision");
    }
    switch (magic) {
      case 5: depth= 1; break;
      case 6: depth= 3; break;
      default: error("image_read_pnm: Invalid depth.");
    }
  } else if (magic == 7) { // PAM
    while (1) {
      tok= read_next_token(file);
      if (EQ(tok, "DEPTH"))
      { depth= atoi(read_next_token(file)); }
      else
      if (EQ(tok, "ENDHDR")) break;
      else
      if (EQ(tok, "HEIGHT"))
      { height= atoi(read_next_token(file)); }
      else
      if (EQ(tok, "MAXVAL"))
      { prec= atoi(read_next_token(file)); }
      else
      if (EQ(tok, "WIDTH"))
      { width= atoi(read_next_token(file)); }
      else
      if (EQ(tok, "TUPLTYPE")) {
        tok= read_next_token(file);
        if (EQ(tok, "GRAYSCALE")) depth= 1;
        else
        if (EQ(tok, "GRAYSCALE_ALPHA")) depth= 2;
        else
        if (EQ(tok, "RGB")) depth= 3;
        else
        if (EQ(tok, "RGB_ALPHA")) depth= 4;
        else error("image_read_pnm: unknown TUPLTYPE");
        c= '#';
        while (c != '\n') i= fscanf(file, "%c", &c);
      }
    }
    i= fscanf(file, "%c", &c);
    if (c != '\n') 
    { error("image_read_pnm: missing newline after ENDHDR"); }
  }
  if (prec != 255)
  { error("image_read_pnm: Precision != 255.");
  }
  assert(width > 0 && height > 0);
  assert(1 <= depth && depth <= 4);
  im= image_make(depth, width, height);
  int opaque= depth % 2;
  buf= malloc(width * depth);
  gray *p[4];
  // for grayscale, RGB, RGBA 
  p[0]= im->chan[1];
  p[1]= im->chan[2];
  p[2]= im->chan[3];
  p[3]= im->chan[0];
  // for gray+alpha
  if (depth == 2) p[1]= p[3]; // alpha
  //
  for (y= 0; y < height; y++) {
    if (width > fread(buf, depth, width, file)) {
      error("image_read_pnm: Unexpected EOF");
    }
    ps= buf;
    for (x= 0; x < width; x++) {
      for (z= 0; z < depth; z++, ps++) {
        *p[z]= *ps;
        p[z]++;
      }
    }
  }
  free(buf);
  return im;
}

void image_write_pnm(image *im, FILE *file) {
  int x, y;
  int width= im->width, height= im->height;
  uchar *buf, *pt;
  float v;

  int z, depth= image_depth(im), opaque= depth % 2;
  assert(file);
  if (opaque) {
    if (depth == 1) fprintf(file, "P5\n");
    else
    if (depth == 3) fprintf(file, "P6\n");
    else error("image_write_pnm: unsupported depth");
    fprintf(file, "%d %d\n255\n", width, height);
  } else {
    fprintf(file, "P7\n");
    fprintf(file, "WIDTH %d\n", width);
    fprintf(file, "HEIGHT %d\n", height);
    fprintf(file, "DEPTH %d\n", depth);
    fprintf(file, "MAXVAL 255\n");
    fprintf(file, "TUPLTYPE ");
    if (depth == 2) fprintf(file, "GRAYSCALE_ALPHA\n");
    else
    if (depth == 4) fprintf(file, "RGB_ALPHA\n");
    else error("image_write_pnm: unsupported depth");
    fprintf(file, "ENDHDR\n");
  }
  buf= malloc(width * depth * sizeof(*buf));
  assert(buf);
  gray *p[4];
  // for grayscale, RGB, RGBA 
  p[0]= im->chan[1];
  p[1]= im->chan[2];
  p[2]= im->chan[3];
  p[3]= im->chan[0];
  // for gray+alpha
  if (depth == 2) p[1]= p[3]; // alpha
  //
  for (y= 0; y < height; y++) {
    pt= buf;
    for (x= 0; x < width; x++) {
      for (z= 0; z < depth; z++, pt++) {
        v= *p[z]; p[z]++;
        if (v < 0) *pt= 0;
        else if (v > 255) *pt= 255;
        else *pt= v;
      }
    }
    if (width > fwrite(buf, depth, width, file)) error("Error writing file.");
  }
  free(buf);
}

#define CMDLEN 256
char cmd[CMDLEN];
char *jpegtopnm= "jpegtopnm -quiet ";
char *pngtopam= "pngtopam -quiet -alphapam ";
char *pngtopnm= "pngtopam -quiet ";
char *pnmtojpeg= "pnmtojpeg -quiet -quality 90 > ";
char *pnmtopng= "pamtopng -quiet > ";

image *image_read(char *fname) {
  FILE *f;
  image *img;
  if (! fname) return image_read_pnm(stdin);
  int i, l= strlen(fname);
  char *ext= fname + l - 4;
  char *filter= NULL;
  unsigned char type;
  if (l >= 4) {
    if (EQ(ext, ".jpg")) filter= jpegtopnm;
    else if (EQ(ext, ".png")) {
      f= fopen(fname, "rb");
      if (! f) error1("image_read: can't read file ", fname);
      fseek(f, 25, SEEK_SET);
      i= fscanf(f, "%c", &type);
      if (type & 4) filter= pngtopam;
      else filter= pngtopnm;
    }
  }
  if (filter) {
    assert(CMDLEN > strlen(filter));
    strcpy(cmd, filter);
    if (strlen(filter) + l >= CMDLEN) {
      error1("image_read: name too long: %s", fname);
    }
    strcat(cmd, fname);
    f= popen(cmd, "r");
    if (! f) error1("image_read: popen failed:", strerror(errno));
    img= image_read_pnm(f);
    pclose(f);
  } else {
    f= fopen(fname, "rb");
    if (! f) error1("image_read: can't read file ", fname);
    img= image_read_pnm(f);
    fclose(f);
  }
  return img;
}

void write_image(image *im, char *fname) {
  FILE *f;
  if (! fname)
  { image_write_pnm(im, stdout); return; }
  int l= strlen(fname);
  char *ext= fname + l - 4;
  char *filter= NULL;
  if (l >= 4) {
    if (EQ(ext, ".jpg")) filter= pnmtojpeg;
    else if (EQ(ext, ".png")) filter= pnmtopng;
  }
  if (filter) {
    assert(CMDLEN > strlen(filter));
    strcpy(cmd, filter);
    if (strlen(filter) + strlen(fname) >= CMDLEN) {
      error1("image_write: name too long: %s", fname);
    }
    strcat(cmd, fname);
    f= popen(cmd, "w");
    if (! f) error1("image_write: popen failed:", strerror(errno));
    image_write_pnm(im, f);
    pclose(f);
  } else {
    f= fopen(fname, "wb");
    if (! f) error1("image_write: can't write file", fname);
    image_write_pnm(im, f);
    fclose(f);
  }
}

image *image_from_channel(image *im, int z) {
  long int l= sizeof(gray) * im->width * im->height;
  if (! im->chan[z]) error("image_from_channel: NULL channel");
  image *om= image_clone(im, 1, 0, 0);
  assert(image_depth(om) == 1);
  assert(om->chan[1]);
  memcpy(om->chan[1], im->chan[z], l);
  assert(! om->chan[0]);
  return om;
}

// vim: set et sw=2:

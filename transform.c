#include "common.h"

image *image_rotate_90(image *im, int angle) {
  int w= im->width, h= im->height;
  int x, y, z, dx, dy;
  image *om= image_clone(im, 0, h, w);
  om->ex= im->ex;
  om->pag= im->pag;
  gray *i, *o;
  for (z= 0; z < 5; z++) {
    i= im->chan[z];
    if (! i) continue;
    o= om->chan[z];
    switch ((int)angle) {
      case 90:
      case -270:
        o += h - 1; dx= h; dy= -1;
        break;
      case 180:
      case -180:
        om->width= w; om->height= h;
        o += h * w - 1; dx= -1; dy= -w;
        break;
      case 270:
      case -90:
        o += h * w - h; dx= -h; dy= 1;
        break;
      default: assert(0);
    }
    for (y= 0; y < h; y++) {
      for (x= 0; x < w; x++) {
        *o= *i; i++; o += dx;
      }
      o += dy - w * dx;
    }
  }
  return om;
}

image *rotate_image(image *im, float angle) {
  float r= angle / 90;
  int n= roundf(r);
  r= angle - (90 * n);
  n= (n % 4) * 90;
  return image_rotate_90(im, n);
}

void splitx_image(void **out1, void **out2, image *im, float x) {
  if (x <= 0) error("x must be > 0.");
  if (x == 1) error("x must be != 1.");
  if (x > 1) x= 1/x;
  gray *p, *p1, *p2;
  uint z, y, h= im->height;
  uint w= im->width;
  uint w1= w * x;
  uint w2= w - w1;
  image* im1= image_clone(im, 0, w1, h);
  image* im2= image_clone(im, 0, w2, h);
  for (z= 0; z < 5; z++) {
    p= im->chan[z];
    if (! p) continue;
    p1= im1->chan[z];
    p2= im2->chan[z];
    for (y= 0; y < h; y++, p += w, p1 += w1, p2 += w2) {
      memcpy(p1, p, w1 * sizeof(gray));
      memcpy(p2, p + w1, w2 * sizeof(gray));
    }
  }
  im1->pag= im->pag;
  im2->pag= im->pag + 1;
  *out1= im1;
  *out2= im2;
}

void splity_image(void **out1, void **out2, image *im, float y) {
  if (y <= 0) error("y must be > 0.");
  if (y == 1) error("y must be != 1.");
  if (y > 1) y= 1/y;
  uint z, w= im->width;
  ulong l1, l2; 
  uint h1= im->height * y;
  uint h2= im->height - h1;
  image* im1= image_clone(im, 0, w, h1);
  image* im2= image_clone(im, 0, w, h2);
  l1= w * h1;
  l2= w * h2;
  gray *p;
  for (z= 0; z < 5; z++) {
    p= im->chan[z];
    if (! p) continue;
    memcpy(im1->chan[z], p, l1 * sizeof(gray));
    memcpy(im2->chan[z], p + l1, l2 * sizeof(gray));
  }
  im1->pag= im->pag;
  im2->pag= im->pag + 1;
  *out1= im1;
  *out2= im2;
}

image *image_crop(image *im, int x1, int y1, int x2, int y2) {
  int z;
  int w= im->width;
  int h= im->height;
  int w1= x2 - x1;
  int h1= y2 - y1;
  int i;
  gray *s, *t;
  // 0 <= x1 < x2 <= im->width  
  // 0 <= y1 < y2 <= im->height  
  if (x1 < 0 || x2 <= x1 || x2 > w) error("crop: wrong x parameters\n");
  if (y1 < 0 || y2 <= y1 || y2 > h) error("crop: wrong y parameters\n");
  image *out= image_clone(im, 0, w1, h1);
  out->ex= im->ex;
  out->pag= im->pag;
  for (z= 0; z < 5; z++) {
    if (! im->chan[z]) continue;
    for (i= 0; i < h1; i++) {
      s= im->chan[z] + (i + y1) * w + x1; 
      t= out->chan[z] + i * w1;
      memcpy(t, s, w1 * sizeof(*s));
    }
  }
  return out;
}

void skew_image(image* im, real angle) {
  angle *= M_PI /180;
  if (fabs(angle) > 45) error("skew: angle must be between -45 and 45.");
  real b= sin(angle);
  real a= b / (1 + cos(angle));
  shearx_image(im, a);
  sheary_image(im, b);
  shearx_image(im, a);
}

real skew_score(int d, image *test, vector *v) {
  int i, j, x, y;
  int  w= test->width, h= test->height;
  real t, *p, *end;
  gray *pt;
  clear_vector(v);
  for (y= 0; y < h; y++) {
    pt= test->chan[1] + y * w;
    x= 0;
    for (i= 0; i <= abs(d); i++) {
      if (d >= 0) {j= y + i;} else {j= y + w - i;} 
      for (; x < w * (i + 1) / (abs(d) + 1); x++, pt++) {
        if (*pt) {v->data[j] += fabs(*pt);}
      }
    }
  }
  t= 0;
  for (p= v->data, end= p + v->size; p < end; p++) { t += *p * *p; }
  return t;
}

real detect_skew_image(image *im) {
  int i, y, w1, w= im->width;
  int h= im->height;
  real t, s= 0;
  // create test image
  image *test= image_clone(im, 0, w, h-1);
  gray *p1, *p2, *pt, *end;
  for (y= 0; y < h - 1; y++) {
    p1= im->chan[1] + y * w;
    end= p2= p1 + w;
    pt= test->chan[1] + y * w;
    for (; p1 < end; p1++, p2++, pt++) {
      t= fabs(*p1 - *p2);
      *pt= t;
      s += t * t;
    }
  }
  s= sqrt(s / w / (h-1));
  for (y= 0; y < h - 1; y++) {
    pt= test->chan[1] + y * w;
    end= pt + w;
    for (; pt < end; pt++) {
      if (*pt < s) {*pt= 0;}
    }
  }
  vector *v=  make_vector(h + w);
  int a, b;
  real sa, sb;
  a= w / 10; b= -a;
  sa= skew_score(a, test, v); sb= skew_score(b, test, v);
  while (abs(a - b) > 1) {
    if (sa > sb) {
      b= (5 * a - 2 * b) / 3;
      sb= skew_score(b, test, v);
    } else {
      a= (5 * b - 2 * a) / 3;
      sa= skew_score(a, test, v);
    }
  }
  if (sb > sa) {sa= sb; a= b;}
  if (a == 0) t= 0;
  if (a > 0) t= a + 1;
  if (a < 0) t= a - 1;
  return atan(t / w) * 180 / M_PI;
}

void shearx_image(image *im, real t) {
  // t= tan(angle * M_PI / 180);
  assert(fabs(t) <= 1);
  uint h= im->height;
  uint w= im->width;
  uint y, z;
  int di;
  real dr, df, ca, cb, cc, cd;;
  gray *end, *p, *a, *b;
  gray *buf= malloc(w * sizeof(*buf));
  for (z=0; z<5; z++) {
    if (! im->chan[z]) continue;
    for (y= 0; y < h; y++) {
      memcpy(buf, im->chan[z] + (w * y), w * sizeof(*buf));
      dr= ((int)y - (int)h/2) * t;
      di= floor(dr);
      df= dr - di;
      if (di > 0) {
        cb= df; ca= 1 - cb;
        p= im->chan[z] + (w * y);
        end= p + w;
        b= buf + di;
        a= b - 1;
        for (; p + di < end; p++, a++, b++) *p= cb * *b + ca * *a;
        for (; p < end; p++) *p= *(p-1);
      } else {
        cb= df; ca= 1 - cb;
        p= im->chan[z] + (w * y) + w - 1;
        end= p - w;
        b= buf + w - 1 + di;
        a= b - 1;
        for (; p + di - 1 > end; p--, a--, b--) *p= cb * *b + ca * *a;
        for (; p > end; p--) *p= *(p+1);
      }
    }
  }
  free(buf);
}

void sheary_image(image *im, real t) {
  // t= tan(angle * M_PI / 180);
  assert(fabs(t) <= 1);
  uint w= im->width;
  uint h= im->height;
  int z;
  gray *p, *end;
  gray *buf= malloc(w * sizeof(*buf));
  int d, *di= malloc(w * sizeof(*di));
  real f, dr, *df= malloc(w * sizeof(*df));
  int a, b, x, y;
  for (x= 0; x < w; x++) {
    dr= ((int)w/2 - (int)x) * t;
    di[x]= floor(dr) * w; // vertical offset = dr rows
    df[x]= dr - floor(dr);
  }
  for (z=0; z<5; z++) {
    if (! im->chan[z]) continue;
    end= im->chan[z] + (w * h);
    // down
    if (t > 0) {a= 0; b= w/2;}
    else  {a= w/2; b= w+1;}
    for (y= 0; y < h; y++) {
      p= im->chan[z] + (y * w) + a;
      for (x= a; x < b; x++, p++) {
        d= di[x];
        f= df[x];
        if (p + d + w < end) {
          buf[x]= *(p+d)*(1-f) + *(p+d+w)*f;
        } else {
          buf[x]= *(end-w+x);
        }
      }
      memcpy(
        im->chan[z] + y * w + a,
        buf + a ,
        (b - a) * sizeof(*buf)
      );
    }
    if (t > 0) {a= w/2; b= w+1;}
    else  {a= 0; b= w/2;}
    // up
    for (y= h - 1; y >= 0; y--) { // y MUST be signed!
      p= im->chan[z] + (y * w) + a;
      for (x= a; x < b; x++, p++) {
        d= di[x];
        f= df[x];
        if (p + d + w >= end) {
          buf[x]= *p;
        }
        else
        if (p + d >= im->chan[z]) {
          buf[x]= *(p+d)*(1-f) + *(p+d+w)*f;
        }
        else {
          buf[x]= *(im->chan[z]+x);
        }
      }
      memcpy(
        im->chan[z] + y * w + a,
        buf + a ,
        (b - a) * sizeof(*buf)
      );
    }
  }
  free(buf);
  free(di);
  free(df);
}

int find_margin(vector *v, int w) {
  real t, t1, n1;
  real *p= v->data;
  int i, j, a, b;
  int l= v->len;
  vector *v1;
  if (w < 0 || w > l) error("find_margin: invalid width");
  t= 0;
  // -> logarithmic scale
  for (i= 0; i < l; i++) {
    p[i]= log(p[i] + 1);
    t += p[i];
  }
  // threshold:
  t /= l;
  n1= t1= 0;
  for (i= 0; i < l; i++) {
    if (p[i] <= t) {n1++; t1 += p[i];}
  }
  t= (t + t1/n1) / 2;
  // calc scores for margins
  // (> l: forbidden)
  j= 0;
  for (i= 0; i < l; i++) {
    if (p[i] > t) {j= 0; p[i]= l + 1;}
    else {p[i]= ++ j;}
  }
  j= 0;
  for (i= l - 1; i >=0; i--) {
    if (p[i] > l) j= 0;
    else {p[i] -= ++ j;}
  }
  t= -l;
  for (i= 0; i + w + 1 < l; i++) {
    a= p[i];
    if (a > l) continue;
    b= p[i + w + 1];
    if (b > l) continue;
    p[i]= a -= b;
    if (a > t) {t= a; j= i;}
  }
  i= j; while (p[i] == t) i++;
  return (j + i) / 2;
}

// vim: sw=2 ts=2 sts=2 expandtab:

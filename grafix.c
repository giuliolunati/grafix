#include "common.h"
#include <errno.h>

#define EQ(a, b) (0 == strcmp((a), (b)))
#define IS_IMAGE(p) (((image *)p)->magic == 'I')
#define IS_VECTOR(p) (((vector *)p)->magic == 'V')

void help(char **arg0, char *topic) {
#define TOPIC(x) EQ((x), topic)
if (! topic) {
  printf("\nUSAGE: %s COMMANDS...\n\n", *arg0);
  printf(
"=================== COMMANDS: ==================\n\
HELP: \n\
  = -h, --help ------------------ this summary\n\
STACK:\n\
  = quit ------------------- quit w/out output\n\
  - pop ------------------------- discard item\n\
  = swap --------------- swap 1st and 2nd item\n\
  + unpop --------------------------- undo pop\n\
CONTROL:\n\
  = odd ------ the following apply to odd page\n\
  = even ---- the following apply to even page\n\
  = all ------------------ end 'even' or 'odd'\n\
INPUT, OUTPUT:\n\
  + - ------------------ load image from STDIN\n\
  + FILENAME.EXT -------- load image from file\n\
  - w FILENAME/- -------- write to file/stdout\n\
CREATE:\n\
  + copy -------------------------------- copy\n\
  + image DEPTH WIDTH HEIGHT ------- new image\n\
  + clone DEPTH WIDTH HEIGHT ----- clone image\n\
SCALE, TRANSFORM:\n\
  ~ cropx LEFT RIGHT ------- crop horizontally\n\
  ~ cropy TOP BOTTOM --------- crop vertically\n\
  +~splitx {X | N} ---------- split vertically\n\
  +~splity {Y | N} -------- split horizontally\n\
  = deskew ---------------------- deskew image\n\
  = skew ANGLE ----------- rotate (-45 ... 45)\n\
  ~ rot* ANGLE ---- rotate (only +-90 180 270)\n\
  ~ double HARDNESS ----------- double up size\n\
  ~ half ---------------------- half down size\n\
LIGHT, COLOR:\n\
  - diff ---------------------- diff im2 - im1\n\
  + bg ----------------------- find background\n\
  = fix-bg -------------------- fix background\n\
  - div* -------------------- divide im2 / im1\n\
  = bin {auto | THRESHOLD} ---- to black&white\n\
  = dither STEP -------------------- dithering\n\
  = con* BLACK WHITE -------- enhance contrast\n\
  = darker FILENAMES... -- darker of all pixel\n\
SELECT, DRAW:\n\
  = rect* VAL X Y X' Y' ----- select rectangle\n\
  = fill A R G B -------------- fill selection\n\
  = grid STEP ----------- draw grid over image\n\
  - s-paste ------------------- seamless paste\n\
COLORSPACE, CHANNELS:\n\
  = alpha ------------------ add alpha channel\n\
  = opaque -------------- remove alpha channel\n\
  + chan* N ------------- extract n-th channel\n\
FILTERS:\n\
  = lapl* ----------------- negative laplacian\n\
  - pois* TOL ------------------ solve poisson\n\
ANALYSIS:\n\
  + histo* CHANNEL ----------------- histogram\n\
  = stat* --------------------- calc statstics\n\
PAGES, OCR\n\
  = pag* NUM ----------------- set page number\n\
  = ex HEIGHT ----------- set lowercase height\n\
\n");
} else printf("No help for %s.\n", topic);
exit(0);
}

int get_page_number(char *s) {
  int n;
  for (n= 0; *s; s++) {
    if ('.' == *s && n) break;
    if ('0' <= *s && *s <= '9') {
      n= n * 10 + (*s - '0');
    }
    else n= 0;
  }
  return n;
}

//// STACK ////
#define STACK_SIZE 256
void *stack[STACK_SIZE];
#define SP stack[sp]
#define SP1 stack[sp - 1]
#define SP2 stack[sp - 2]
#define SP3 stack[sp - 3]

int sp= 0;

void swap() {
  void *p;
  if (sp < 2) error("Stack underflow");
  p= SP2; SP2= SP1; SP1= p;
}

void *pop() {
  if (sp < 1) error("Stack underflow");
  sp--;
  return SP;
}

void push(void *p) {
  if (sp >= STACK_SIZE) error("Stack overflow");
  if (SP) free(SP);
  SP= p;
  sp++;
}

void unpop() {
  if (sp >= STACK_SIZE) error("Stack overflow");
  sp++;
}

image *im(int i) {
  if (sp < i) error("Missing image");
  return (image*)stack[sp - i];
}

char type(char *a) {
  if (!a) return 0;
  if (strchr(a, '.')) {
    if (EQ(a, "0.0") || EQ(a, "-0.0") || atof(a)) return 'd'; // decimal
    else return 'f'; // filename
  } else {
    if (EQ(a, "0") || EQ(a, "-0") || atoi(a)) return 'i'; // integer
    else if (EQ(a, "-")) return '-'; // null
    else return 'w'; // word
  }
}

//// MAIN ////
int main(int argc, char **args) {
  #define ARG_EQ(x) EQ((x), *arg)
  #define ARG_HEAD(x) (strstr(*arg,x) == *arg)
  #define ARG_LT(x) (strcmp(*arg,x) < 0)
  #define CMD_LEN 300
  #define BUF_LEN 200
  char **arg= args;
  char name[200];
  char cmd[256];
  char *ext;
  char *jpegtopnm= "jpegtopnm -quiet ";
  char *pnmtojpeg= "pnmtojpeg -quiet -quality 90 > ";
  image *img;
  vector *v;
  void *p;
  FILE *f;
  real x, y, t[9];
  int c, l, i, w, h;

  if (argc < 2) help(args, NULL);
  while (*(++arg)) {
    if (strchr(*arg, '.')) { // FILENAME.EXT
      i= get_page_number(*arg);
      if (i > 9999) error("page number > 9999");
      img= image_read(*arg);
      img->pag= i;
      push(img);
    }
    else
    if (ARG_LT("d")) { // !* - c*
      if (ARG_EQ("-")) {
        push(image_read(NULL)); // stdin
      }
      else
      if (ARG_EQ("-h") || ARG_EQ("--help")) {
        help(args, *(++arg));
      }
      else
      if (ARG_EQ("all")); // see odd, even
      else
      if (ARG_EQ("alpha")) add_channel(im(1), 0);
      else
      if (ARG_EQ("bg")) { // FLOAT
        arg++;
        c= type(*arg); 
        if (! c) error("bg: missing parameter");
        if (c != 'i' && c != 'f') error("bg: expected number");
        push(image_background(im(1), atof(*arg)));
      }
      else
      if (ARG_EQ("bin")) { // auto | FLOAT
        if (! *(++arg)) error("bin: missing parameter");
        if (ARG_EQ("auto")) {
          if (im(1)->graythr < 0) {
            calc_statistics(im(1), 0);
          }
          x= im(1)->graythr;
        }
        else x= atof(*arg);
        x > 1 || (x *= 255);
        contrast_image(im(1), x, x);
      }
      else
      if (ARG_HEAD("chan")) { // FLOAT
        arg++;
        c= type(*arg); 
        if (! c) error("channel: missing parameter");
        if (c != 'i') error("channel: expected number");
        i= atoi(*arg);
        if (i < 0) error("channel: invalid parameter");
        push(image_from_channel(im(1), i));
      }
      else
      if (ARG_EQ("clone")) { // DEPTH WIDTH HEIGHT
        for (i= 0; i < 3; i++) {
          arg++;
          c= type(*arg); 
          if (! c) error("clone: missing parameter");
          if (c != 'i') error("clone: expected int");
          t[i]= atof(*arg);
        }
        push(image_clone(im(1),t[0],t[1],t[2]));
      }
      else
      if (ARG_HEAD("con")) { // auto | BLACK WHITE
        if (! *(++arg)) error("contrast: missing BLACK parameter");
        if (ARG_EQ("auto")) {
          if (im(1)->black < 0 || im(1)->white < 0) {
            calc_statistics(im(1), 0);
            x= im(1)->black;
            y= im(1)->white;
          }
        } else {
          if (! *(++arg)) error("contrast: missing WHITE parameter");
          x= atof(*(arg-1));
          y= atof(*arg);
        }
        contrast_image(im(1), x, y);
      }
      else
      if (ARG_EQ("copy")) push(image_copy(im(1)));
      else
      if (ARG_EQ("cropx")) { // FLOAT FLOAT
        img= im(1);
        if (! *(++arg)) error("cropx: missing LEFT parameter");
        x= atof(*arg);
        if (x <= 1) x *= img->width;
        if (x < 0 || x >= img->width) error("cropx: invalid LEFT parameter");
        if (! *(++arg)) error("cropx: missing RIGHT parameter");
        y= atof(*arg);
        if (y <= 1) y *= img->width;
        if (y <= x || y > img->width) error("cropx: invalid RIGHT parameter");
        push(image_crop(img, x, 0, y, img->height));
        swap(); pop();
      }
      else
      if (ARG_EQ("cropy")) { // FLOAT FLOAT
        img= im(1);
        if (! *(++arg)) error("cropy: missing TOP parameter");
        x= atof(*arg);
        if (x <= 1) x *= img->height;
        if (x < 0 || x >= img->height) error("cropy: invalid TOP parameter");
        if (! *(++arg)) error("cropy: missing BOTTOM parameter");
        y= atof(*arg);
        if (y <= 1) y *= img->height;
        if (y <= x || y > img->height) error("cropy: invalid BOTTOM parameter");
        push(image_crop(img, 0, x, img->width, y));
        swap(); pop();
      }
      else
      goto command_not_found;
    }
    else
    if (ARG_LT("g")) { // d* - f*
      if (ARG_EQ("darker")) {
        i= 1;
        while (strchr(*(++arg), '.')) { // FILENAME.EXT
          img= image_read(*arg);
          if (i) { push(img); i= 0; }
          else { darker_image(im(1), img); }
        }
        arg--;
      }
      else
      if (ARG_EQ("deskew")) {
        x= detect_skew_image(im(1));
        fprintf(stderr, "skew: %g\n", x);
        skew_image(im(1), x);
      }
      else
      if (ARG_EQ("diff")) {
        diff_image(im(2), im(1));
        pop();
      }
      else
      if (ARG_EQ("dither")) { // INT
        arg++;
        c= type(*arg);
        if (! c) error("dither: missing STEP parameter");
        if (c == 'i' || c == 'f') x= atof(*arg);
        else error("dither: wrong parameter");
        dither_floyd_bidir(im(1), x);
      }
      else
      if (ARG_HEAD("div")) {
        divide_image(im(2), im(1));
        pop();
      }
      else
      if (ARG_EQ("double")) { // FLOAT
        if (! *(++arg)) error("double: missing HARDNESS parameter");
        x= atof(*arg);
        push(image_double(im(1), x));
        swap(); pop();
      }
      else
      if (ARG_EQ("even")) {
        if (! *(++arg)) error("even: missing args");
        if (1 == img->pag % 2) { // odd
          while (*arg && ! ARG_EQ("odd") && ! ARG_EQ("all")) arg++;
        }
        arg--;
      }
      else
      if (ARG_EQ("ex")) { // FLOAT
        if (! *(++arg)) error("ex: missing parameter");
        x= atof(*arg);
        if (x <= 0) error("ex param must be > 0.");
        if (sp) {
          img= im(1);
          if (x < 1) x *= img->height;
          img->ex= x;
        }
        default_ex= x;
      }
      else
      if (ARG_EQ("fill")) { // A,R,G,B
        for (i= 0; i < 4; i++) {
          arg++;
          if (! *arg) error("fill: missing parameter");
          if (type(*arg) == 'i') t[i]= atoi(*arg);
          else if (type(*arg) == 'd') t[i]= atof(*arg) * 255;
          else if (EQ(*arg, "-")) t[i]= NAN;
          else error("fill: wrong parameter");
        }
        fill_selection(im(1),t[0],t[1],t[2],t[3]);
      }
      else
      if (ARG_EQ("fix-bg")) { // NUMBER
        arg++;
        c= type(*arg); 
        if (! c) error("fix-bg: missing parameter");
        if (c != 'i' && c != 'f') error("fix-bg: expected number");
        push(image_background(im(1), atof(*arg)));
        divide_image(im(2), im(1));
        pop();
      }
      else
      goto command_not_found;
    }
    else
    if (ARG_LT("r")) { // g* - q*
      if (ARG_EQ("grid")) {
        img= im(1);
        if (! *(++arg)) error("grid: missing STEP parameter");
        x= atof(*arg);
        y= x;
        if (x <= 1) x *= img->width;
        if (y <= 1) y *= img->height;
        draw_grid(im(1), x, y);
      }
      else
      if (ARG_EQ("half")) {
        push(image_half(im(1)));
        swap(); pop();
      }
      else
      if (ARG_HEAD("histo")) { // INT
        if (! *(++arg)) error("histo: missing channel");
        i= atoi(*arg);
        v= histogram_of_image(im(1), 0);
        push(v);
      }
      else
      if (ARG_EQ("image")) { // DEPTH WIDTH HEIGHT
        for (i= 0; i < 3; i++) {
          arg++;
          c= type(*arg); 
          if (! c) error("image: missing parameter");
          if (c != 'i') error("image: expected int");
          t[i]= atof(*arg);
        }
        push(image_make(t[0],t[1],t[2]));
      }
      else
      if (ARG_HEAD("lapl")) {
        laplacian(im(1), -0.25);
      }
      else
      if (ARG_EQ("odd")) {
        if (! *(++arg)) error("odd: missing args");
        if (0 == img->pag % 2) { // even
          while (*arg && ! ARG_EQ("even") && ! ARG_EQ("all")) arg++;
        }
        arg--;
      }
      else
      if (ARG_EQ("opaque")) {
        img= im(1);
        if (img->ALPHA) free(img->ALPHA);
        img->ALPHA= NULL;
      }
      else
      if (ARG_HEAD("pag")) { // INT
        if (! *(++arg)) error("pag: missing parameter");
        im(1)->pag= atoi(*arg);
        if (im(1)->pag > 9999) error("pag: number > 9999");
      }
      else
      if (ARG_HEAD("pois")) {
        if (! *(++arg)) error("poisson: missing PRECISION");
        x= atof(*arg);
        solve_poisson(im(2), im(1), -0.25, 0, x);
        pop();
      }
      else
      if (ARG_EQ("pop")) pop();
      else
      if (ARG_EQ("quit")) exit(0);
      else
      goto command_not_found;
    }
    else
    if (ARG_LT("~~")) { // r* - ~*
      if (ARG_HEAD("rect")) { // VAL X0 Y0 X1 Y1
        img= im(1);
        for (i= 0; i < 5; i++) {
          arg++;
          c= type(*arg); 
          if (! c) error("rectangle: missing parameter");

          t[i]= atof(*arg);
          if (c == 'i') ;
          else
          if (c == 'd') {
            if (i == 0) ;
            else if (i % 2) t[i] *= img->width;
            else t[i] *= img->height;
          }
          else error("rectangle: wrong parameter");
        }
        select_rectangle(im(1),t[0],t[1],t[2],t[3],t[4]);
      }
      else
      if (ARG_HEAD("rot")) { // +-90, 180, 270
        if (! *(++arg)) error("rot: missing parameter");
        push(rotate_image(im(1), atof(*arg)));
        swap(); pop();
      }
      else
      if (ARG_EQ("s-paste")) {
        laplacian(im(1), -0.25);
        solve_poisson(im(2), im(1), -0.25, 0, 0.01);
        pop();
      }
      else
      if (ARG_EQ("skew")) { // FLOAT
        if (! *(++arg)) error("skew: missing parameter");
        skew_image(im(1), atof(*arg));
      }
      else
      if (ARG_EQ("splitx")) { // FLOAT
        if (! *(++arg)) error("splitx: missing parameter");
        img= im(1);
        push(0); swap();
        push(0); swap();
        splitx_image(&SP2, &SP3, img, atof(*arg));
        pop();
      }
      else
      if (ARG_EQ("splity")) { // FLOAT
        if (! *(++arg)) error("splity: missing parameter");
        img= im(1);
        push(0); swap();
        push(0); swap();
        splity_image(&SP2, &SP3, img, atof(*arg));
        pop();
      }
      else
      if (ARG_HEAD("stat")) {
        calc_statistics(im(1), 1);
      }
      else
      if (ARG_EQ("swap")) swap();
      else
      if (ARG_EQ("test")) {
        if (! *(++arg)) error("test: missing parameter");
        x=  atof(*arg);
        convolve_3x3(im(1), 4, -1, -1, 0);
      }
      else
      if (ARG_EQ("unpop")) unpop();
      else
      if (ARG_EQ("w")) { // FILENAME
        if (! *(++arg)) error("w: missing filename");
        p= pop();
        if (IS_IMAGE(p)) {
          img= p;
          if (strlen(*arg) >= 200) {
            error1("w: file name too long:", *arg);
          }
          if (EQ(*arg, "-")) {
            write_image(img, NULL);
          } else {
            sprintf(name, *arg, img->pag);
            write_image(img, name);
          }
        }
        else
        if (IS_VECTOR(p)) {
          if (EQ(*arg, "-")) {
            f= stdout;
            write_vector(p, f);
          } else {
            f= fopen(*arg, "wb");
            write_vector(p, f);
            fclose(f);
          }
        }
      }
      else goto command_not_found;
    }
    else goto command_not_found;
  }
  goto end;
command_not_found:
    error1("Command not found:", *arg);
end: ;
}

// vim: sw=2 ts=2 sts=2 expandtab:

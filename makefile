CC = gcc
CFLAGS = -O3
LDFLAGS = -O3
OBJS = vector.o image.o transform.o scale.o convolution.o pixel.o draw.o select.o dithering.o poisson.o detect.o

grafix: grafix.c libgrafix.a common.h
	$(CC) $(LDFLAGS) -o $@ $< libgrafix.a -lm 

libgrafix.a: ${OBJS}
	ar rs $@ $(OBJS)

test: test.c libgrafix.a common.h
	$(CC) $(LDFLAGS) -o $@ $< libgrafix.a -lm 

.PHONY: clean
clean:
	rm -f grafix *~ *.o *.a

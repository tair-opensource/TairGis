STD= 
WARN= -Wall -Wno-strict-aliasing -Wno-typedef-redefinition
OPT= -O2 -std=c99

R_CFLAGS= $(STD) $(WARN) $(OPT) $(DEBUG) $(CFLAGS)
R_LDFLAGS= $(LDFLAGS)
DEBUG= -g

R_CC=$(CC) $(R_CFLAGS)
R_LD=$(CC) $(R_LDFLAGS)

all: geom.o grisu3.o rtree.o geoutil.o \
	 poly.o polyinside.o polyraycast.o polyintersects.o \
	 hash.o bing.o json.o
testapp: all
	-@$(R_CC) -o test test.c grisu3.o -I. \
		geom_test.c geom.o \
		rtree_test.c rtree.o \
		geoutil_test.c geoutil.o \
		json.o \
		polyinside_test.c polyintersects_test.c poly_test.c \
			poly.o polyinside.o polyraycast.o polyintersects.o \
		-lm

test: testapp
	-@./test

.PHONY: all

geom.o: geom.h geom.c geom_levels.c geom_polymap.c geom_json.c
grisu3.o: grisu3.h grisu3.c
rtree.o: rtree.h rtree.c rtree_tmpl.c
geoutil.o: geoutil.h geoutil.c
poly.o: poly.h poly.c
polyinside.o: poly.h polyinside.c
polyraycast.o: poly.h polyraycast.c
polyintersects.o: poly.h polyintersects.c
hash.o: hash.h hash.c
bing.o: bing.h bing.c
json.o: json.h json.c

.c.o:
	$(R_CC) -c $<

clean:
	rm -f *.o

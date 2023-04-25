#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "zmalloc.h"
#include "poly.h"
#include "test.h"

static polyMultiPolygon emptyMultiPolygon = {0,0,0};

int insideshpext(polyPoint p, polyPolygon shape, int exterior);

polyPoint P(double x, double y){
	polyPoint p = {x, y};
	return p;
}



void *makeSegment(int count, ...){
	void *segment = zmalloc(4+count*16);
	assert(segment);
	((uint32_t*)segment)[0] = count;
	va_list ap;
	va_start(ap, count);
	for (int i=0;i<count;i++){
		polyPoint p = va_arg(ap, polyPoint);
		*((double*)(segment+4+(i*16)+0)) = p.x;
		*((double*)(segment+4+(i*16)+8)) = p.y;

	}
	va_end(ap);
	return segment;
}

void *makeMultiSegment(int count, ...){
	int sz = 4;
	void *segment = zmalloc(sz);
	assert(segment);
	((uint32_t*)segment)[0] = count;
	va_list ap;
	va_start(ap, count);
	for (int i=0;i<count;i++){
		polyPolygon pp = va_arg(ap, polyPolygon);
		int len = pp.len*16+4;
		sz += len;
		segment = zrealloc(segment, sz);
		assert(segment);
		memcpy(segment+sz-len, ((uint8_t*)pp.values)-4, len);
	}
	va_end(ap);
	return segment;
}

void testRayInside(polyPoint p, polyPolygon ps, int expect) {
	int res = insideshpext(p, ps, 1);
	assert(res == expect);
}

int test_PolyRayInside(){
	void *segment = makeSegment(5, P(0, 0), P(0, 3), P(4, -3), P(4, 0), P(0, 0));
	polyPolygon strange = polyPolygonFromGeomSegment(segment, 2);
	
	// on the edge
	testRayInside(P(0, 0), strange, 1);
	testRayInside(P(0, 3), strange, 1);

	testRayInside(P(4, -3), strange, 1);
	testRayInside(P(4, -2), strange, 1);
	testRayInside(P(3, 0), strange, 1);
	testRayInside(P(1, 0), strange, 1);

	// ouside by just a tad
	testRayInside(P(-0.1, 0), strange, 0);
	testRayInside(P(-0.1, -0.1), strange, 0);
	testRayInside(P(0, 3.1), strange, 0);
	testRayInside(P(0.1, 3.1), strange, 0);
	testRayInside(P(-0.1, 3), strange, 0);
	testRayInside(P(4, -3.1), strange, 0);
	testRayInside(P(3.9, -3), strange, 0);
	testRayInside(P(4.1, -2), strange, 0);
	testRayInside(P(3, 0.1), strange, 0);
	testRayInside(P(1, -0.1), strange, 0);

	zfree(segment);
	return 1;
}

static int globalsMade = 0;
static polyPolygon texterior;
static polyPolygon tholeA;
static polyPolygon tholeB;
static polyMultiPolygon tholes;
static void makeGlobals(){
	if (globalsMade){
		return;
	}
	globalsMade = 1;
	texterior = polyPolygonFromGeomSegment(makeSegment(5, 
		P(0, 0),
		P(0, 6),
		P(12, -6),
		P(12, 0),
		P(0, 0)
	), 2);
	tholeA = polyPolygonFromGeomSegment(makeSegment(4, 
		P(1, 1),
		P(1, 2),
		P(2, 2),
		P(2, 1)
	), 2);
	tholeB = polyPolygonFromGeomSegment(makeSegment(3, 
		P(11, -1),
		P(11, -3),
		P(9, -1)
	), 2);

	tholes = polyMultiPolygonFromGeomSegment(makeMultiSegment(2, 
		tholeA, 
		tholeB
	), 2);
}

typedef struct point {
	polyPoint p;
	int ok; 
} point;


int test_PolyRayExteriorHoles() {
	makeGlobals();


	point ttpoints[] = {
		{P(.5, 3), 1},
		{P(11.5, -4.5), 1},
		{P(6, 0), 1},

		{P(3.5, .5), 1},
		{P(1.5, 1.5), 0},
		{P(10.5, -1.5), 0},
		{P(-2, 0), 0},
		{P(0, -2), 0},
		{P(8, -3), 0},
		{P(8, 1), 0},
		{P(14, -1), 0},
	};

	int count = sizeof(sizeof(ttpoints)/sizeof(point));
	point *points = zmalloc(count*sizeof(point));
	assert(points);
	memcpy(points, ttpoints, count*sizeof(point));

	// add the edges, all should be inside
	for (int i=0;i<texterior.len;i++) {
		points = zrealloc(points, (count+1)*sizeof(point));
		assert(points);
		point p = {polyPolygonPoint(texterior, i), 1};
		points[count] = p;
		count++;
	}
	for (int i=0;i<tholeA.len;i++) {
		points = zrealloc(points, (count+1)*sizeof(point));
		assert(points);
		point p = {polyPolygonPoint(tholeA, i), 1};
		points[count] = p;
		count++;
	}
	for (int i=0;i<tholeB.len;i++) {
		points = zrealloc(points, (count+1)*sizeof(point));
		assert(points);
		point p = {polyPolygonPoint(tholeB, i), 1};
		points[count] = p;
		count++;
	}
	for (int i=0;i<count;i++) {
		int ok = polyPointInside(points[i].p, texterior, tholes);
		assert(ok == points[i].ok);
	}

	zfree(points);

	return 1;
}

int test_PolyInsideShapes() {
	makeGlobals();
	assert(polyPolygonInside(texterior, texterior, emptyMultiPolygon)==1);
	assert(polyPolygonInside(texterior, texterior, tholes)==0);
	assert(polyPolygonInside(tholeA, texterior, emptyMultiPolygon)==1);
	assert(polyPolygonInside(tholeB, texterior, emptyMultiPolygon)==1);
	assert(polyPolygonInside(tholeA, tholeB, emptyMultiPolygon)==0);
	return 1;
}

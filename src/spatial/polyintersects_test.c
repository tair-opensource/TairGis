#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "poly.h"
#include "test.h"

static polyMultiPolygon emptyMultiPolygon = {0,0,0};

polyPoint P(double x, double y);
void *makeSegment(int count, ...);
void *makeMultiSegment(int count, ...);

#define POLYGON(count, ...) (polyPolygonFromGeomSegment(makeSegment(count, __VA_ARGS__), 2))
#define MULTIPOLYGON(count, ...) (polyMultiPolygonFromGeomSegment(makeMultiSegment(count, __VA_ARGS__), 2))

int lineintersects(
	polyPoint a, polyPoint b, // segment 1
	polyPoint c, polyPoint d // segment 2
);

void testIntersectsLinesA(polyPoint a, polyPoint b, polyPoint c, polyPoint d, int expect) {
	int res = lineintersects(a, b, c, d);
	assert(res == expect);
	res = lineintersects(b, a, c, d);
	assert(res == expect);
	res = lineintersects(a, b, d, c);
	assert(res == expect);
	res = lineintersects(b, a, d, c);
	assert(res == expect);
}

void testIntersectsLines(polyPoint a, polyPoint b, polyPoint c, polyPoint d, int expect) {
	testIntersectsLinesA(a, b, c, d, expect);
	testIntersectsLinesA(c, d, a, b, expect);
}

int test_PolyIntersectsLines() {
	testIntersectsLines(P(0, 6), P(12, -6), P(0, 0), P(12, 0), 1);
	testIntersectsLines(P(0, 0), P(5, 5), P(5, 5), P(0, 10), 1);
	testIntersectsLines(P(0, 0), P(5, 5), P(5, 6), P(0, 10), 0);
	testIntersectsLines(P(0, 0), P(5, 5), P(5, 4), P(0, 10), 1);
	testIntersectsLines(P(0, 0), P(2, 2), P(0, 2), P(2, 0), 1);
	testIntersectsLines(P(0, 0), P(2, 2), P(0, 2), P(1, 1), 1);
	testIntersectsLines(P(0, 0), P(2, 2), P(2, 0), P(1, 1), 1);
	testIntersectsLines(P(0, 0), P(0, 4), P(1, 4), P(4, 1), 0);
	testIntersectsLines(P(0, 0), P(0, 4), P(1, 4), P(4, 4), 0);
	testIntersectsLines(P(0, 0), P(0, 4), P(4, 1), P(4, 4), 0);
	testIntersectsLines(P(0, 0), P(4, 0), P(1, 4), P(4, 1), 0);
	testIntersectsLines(P(0, 0), P(4, 0), P(1, 4), P(4, 4), 0);
	testIntersectsLines(P(0, 0), P(4, 0), P(4, 1), P(4, 4), 0);
	testIntersectsLines(P(0, 4), P(4, 0), P(1, 4), P(4, 1), 0);
	testIntersectsLines(P(0, 4), P(4, 0), P(1, 4), P(4, 4), 0);
	testIntersectsLines(P(0, 4), P(4, 0), P(4, 1), P(4, 4), 0);
	return 1;
}

void testIntersectsShapes(polyPolygon exterior, polyMultiPolygon holes, polyPolygon shape, int expect) {
	int got = polyPolygonIntersects(shape, exterior, holes);
	assert(got == expect);
	got = polyPolygonIntersects(exterior, shape, emptyMultiPolygon);
	assert(got == expect);
}


int test_PolyIntersectsShapes() {
	testIntersectsShapes(
		POLYGON(4, P(6, 0), P(12, 0), P(12, -6), P(6, 0)),
		emptyMultiPolygon,
		POLYGON(4, P(0, 0), P(0, 6), P(6, 0), P(0, 0)),
		1);

	testIntersectsShapes(
		POLYGON(4, P(7, 0), P(12, 0), P(12, -6), P(7, 0)),
		emptyMultiPolygon,
		POLYGON(4, P(0, 0), P(0, 6), P(6, 0), P(0, 0)),
		0);

	testIntersectsShapes(
		POLYGON(4, P(0.5, 0.5), P(0.5, 4.5), P(4.5, 0.5), P(0.5, 0.5)),
		emptyMultiPolygon,
		POLYGON(4, P(0, 0), P(0, 6), P(6, 0), P(0, 0)),
		1);

	testIntersectsShapes(
		POLYGON(4, P(0, 0), P(0, 6), P(6, 0), P(0, 0)),
		MULTIPOLYGON(1, POLYGON(5, P(1, 1), P(1, 2), P(2, 2), P(2, 1), P(1, 1))),
		POLYGON(4, P(0.5, 0.5), P(0.5, 4.5), P(4.5, 0.5), P(0.5, 0.5)),
		1);

	testIntersectsShapes(
		POLYGON(5, P(0, 0), P(0, 10), P(10, 10), P(10, 0), P(0, 0)),
		MULTIPOLYGON(1, POLYGON(5, P(2, 2), P(2, 6), P(6, 6), P(6, 2), P(2, 2))),
		POLYGON(5, P(1, 1), P(1, 9), P(9, 9), P(9, 1), P(1, 1)),
		1);
	return 1;
}

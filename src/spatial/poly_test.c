#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "poly.h"
#include "test.h"

polyPoint P(double x, double y);
polyRect RECT(polyPoint min, polyPoint max){
	polyRect r = {min, max};
	return r;
}

int test_PolyRectIntersects() {
	assert(polyRectIntersectsRect(RECT(P(0, 0), P(10, 10)), RECT(P(-1, -1), P(1, 1))));
	assert(polyRectIntersectsRect(RECT(P(0, 0), P(10, 10)), RECT(P(9, 9), P(11, 11))));
	assert(polyRectIntersectsRect(RECT(P(0, 0), P(10, 10)), RECT(P(9, -1), P(11, 1))));
	assert(polyRectIntersectsRect(RECT(P(0, 0), P(10, 10)), RECT(P(-1, 9), P(1, 11))));
	assert(polyRectIntersectsRect(RECT(P(0, 0), P(10, 10)), RECT(P(-1, -1), P(0, 0))));
	assert(polyRectIntersectsRect(RECT(P(0, 0), P(10, 10)), RECT(P(10, 10), P(11, 11))));
	assert(polyRectIntersectsRect(RECT(P(0, 0), P(10, 10)), RECT(P(10, -1), P(11, 0))));
	assert(polyRectIntersectsRect(RECT(P(0, 0), P(10, 10)), RECT(P(-1, 10), P(0, 11))));
	assert(polyRectIntersectsRect(RECT(P(0, 0), P(10, 10)), RECT(P(1, 1), P(2, 2))));
	return 1;
}

int test_PolyRectInside() {
	assert(polyRectInsideRect(RECT(P(1, 1), P(9, 9)), RECT(P(0, 0), P(10, 10))));
	assert(!polyRectInsideRect(RECT(P(-1, -1), P(9, 9)), RECT(P(0, 0), P(10, 10))));

	return 1;
}
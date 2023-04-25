/* Copyright (c) 2016, Josh Baker <joshbaker77@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED by THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include "poly.h"

static polyMultiPolygon emptyMultiPolygon = {0,0,0};

polyPoint lineCenter(polyPoint a, polyPoint b){
   polyPoint p={
      (a.x+b.x)/2, (a.y+b.y)/2
   };

  return p;
}

int lineintersects(
	polyPoint a, polyPoint b, // segment 1
	polyPoint c, polyPoint d // segment 2
){
	// do the bounding boxes intersect?
	// the following checks without swapping values.
	if (a.y > b.y) {
		if (c.y > d.y) {
			if (b.y > c.y || a.y < d.y) {
				return 0;
			}
		} else {
			if (b.y > d.y || a.y < c.y) {
				return 0;
			}
		}
	} else {
		if (c.y > d.y) {
			if (a.y > c.y || b.y < d.y) {
				return 0;
			}
		} else {
			if (a.y > d.y || b.y < c.y) {
				return 0;
			}
		}
	}
	if (a.x > b.x) {
		if (c.x > d.x) {
			if (b.x > c.x || a.x < d.x) {
				return 0;
			}
		} else {
			if (b.x > d.x || a.x < c.x) {
				return 0;
			}
		}
	} else {
		if (c.x > d.x) {
			if (a.x > c.x || b.x < d.x) {
				return 0;
			}
		} else {
			if (a.x > d.x || b.x < c.x) {
				return 0;
			}
		}
	}

	// the following code is from http://ideone.com/PnPJgb
	double cmpx = c.x-a.x;
	double cmpy = c.y-a.y;
	double rx = b.x-a.x;
	double ry = b.y-a.y;
	double cmpxr = cmpx*ry - cmpy*rx;
	if (cmpxr == 0) {
		// Lines are collinear, and so intersect if they have any overlap
		if (!(((c.x-a.x <= 0) != (c.x-b.x <= 0)) || ((c.y-a.y <= 0) != (c.y-b.y <= 0)))) {
			return 0;
		}
		return 1;
	}
	double sx = d.x-c.x;
	double sy = d.y-c.y;
	double cmpxs = cmpx*sy - cmpy*sx;
	double rxs = rx*sy - ry*sx;
	if (rxs == 0) {
		return 0; // Lines are parallel.
	}
	double rxsr = 1 / rxs;
	double t = cmpxs * rxsr;
	double u = cmpxr * rxsr;
	if (!((t >= 0) && (t <= 1) && (u >= 0) && (u <= 1))) {
		return 0;
	}
	return 1;
}

// Intersects detects if a point intersects another polygon
int polyPointIntersects(polyPoint p, polyPolygon exterior, polyMultiPolygon holes){
	return polyPointInside(p, exterior, holes);
}

int polyLinesIntersect(polyPoint a1, polyPoint a2, polyPoint b1, polyPoint b2){
	return lineintersects(a1,a2,b1,b2);	
}

// Intersects detects if a polygon intersects another polygon
int polyPolygonIntersects(polyPolygon shape, polyPolygon exterior, polyMultiPolygon holes) {
	switch (shape.len) {
	case 0:
		return 0;
	case 1:
		switch (exterior.len) {
		case 0:
			return 0;
		case 1:{
			polyPoint shape0 = polyPolygonPoint(shape, 0);
			polyPoint exterior0 = polyPolygonPoint(exterior, 0);
			return shape0.x == exterior0.x && shape0.y == shape0.y;
		}
		default:
			return polyPointInside(polyPolygonPoint(shape, 0), exterior, holes);
		}
	default:
		switch (exterior.len) {
		case 0:
			return 0;
		case 1:
			return polyPointInside(polyPolygonPoint(exterior, 0), shape, holes);
		}
	}
	if (!polyRectIntersectsRect(polyPolygonRect(shape), polyPolygonRect(exterior))) {
		return 0;
	}
	for (int i=0;i<shape.len;i++) {
		for (int j=0;j<exterior.len;j++) {
			polyPoint shapeP1 = polyPolygonPoint(shape, i);
			polyPoint shapeP2 = polyPolygonPoint(shape, (i+1)%shape.len);
			polyPoint exteriorP1 = polyPolygonPoint(exterior, j);
			polyPoint exteriorP2 = polyPolygonPoint(exterior, (j+1)%exterior.len);
			if (lineintersects(
				shapeP1, shapeP2,
				exteriorP1, exteriorP2
			)) {
				return 1;
			}
		}
	}
	for (int i=0;i<holes.len;i++) {
		polyPolygon hole = polyMultiPolygonPolygon(holes, i);
		if (polyPolygonInside(shape, hole, emptyMultiPolygon)) {
			return 0;
		}
	}
	if (polyPolygonInside(shape, exterior, emptyMultiPolygon)) {
		return 1;
	}
	if (polyPolygonInside(exterior, shape, emptyMultiPolygon)) {
		return 1;
	}

	return 0;
}

int polyPolygonContains(polyPolygon shape, polyPolygon exterior, polyMultiPolygon holes) {
	switch (shape.len) {
	case 0:
		return 0;
	case 1:
		switch (exterior.len) {
		case 0:
			return 0;
		case 1:{
			polyPoint shape0 = polyPolygonPoint(shape, 0);
			polyPoint exterior0 = polyPolygonPoint(exterior, 0);
			return shape0.x == exterior0.x && shape0.y == shape0.y;
		}
		default:
			return polyPointInside(polyPolygonPoint(shape, 0), exterior, holes);
		}
	default:
		switch (exterior.len) {
		case 0:
			return 0;
		case 1:
			return polyPointInside(polyPolygonPoint(exterior, 0), shape, holes);
		}
	}
	
	for (int i=0;i<holes.len;i++) {
		polyPolygon hole = polyMultiPolygonPolygon(holes, i);
		if (polyPolygonInside(shape, hole, emptyMultiPolygon)) {
			return 0;
		}
	}
	if (polyPolygonInside(shape, exterior, emptyMultiPolygon)) {
		return 1;
	}
	if (polyPolygonInside(exterior, shape, emptyMultiPolygon)) {
		return 1;
	}

	return 0;
}


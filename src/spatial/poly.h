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

#ifndef POLY_H_
#define POLY_H_

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct polyPoint {
	double x, y;
} polyPoint;

// Rect is rectangle
typedef struct polyRect {
	polyPoint min, max;
} polyRect;


typedef enum polyRayres{
	RAY_OUT  = 0, // outside of the segment.
	RAY_LEFT = 1, // to the left of the segment
	RAY_ON   = 2, // on segment or vertex, special condition
} polyRayres;

typedef struct polyPolygon {
	int len;        // number of points.
	int dims;       // number of dimensions in polygon.
	double *values; // point values. this array will be len*dims in size.
} polyPolygon;

typedef struct polyMultiPolygon {
	int len;        // number of polygons.
	int dims;       // number of dimensions in polygon.
	void *values;   // pointer to the first polygon.
} polyMultiPolygon;


// create a polygon from a geom segment. the segment should be composed of a 
// 32-bit unsigned int with represents the number of points in the polygon,
// and a series of doubles. The dims value should be 2,3,4 depending on how
// many doubles are required to make up a single point.
polyPolygon polyPolygonFromGeomSegment(void *segment, int dims);

// same as above, but for multiple polygons
polyMultiPolygon polyMultiPolygonFromGeomSegment(void *segment, int dims);

const char *polyRayresString(polyRayres r);
polyRayres polyRaycast(polyPoint p, polyPoint a, polyPoint b);
int polyPointInsideRect(polyPoint p, polyRect r);
int polyPolygonInsideRect(polyPolygon pp, polyRect r);
polyPoint polyPolygonPoint(polyPolygon pp, int idx);
polyPolygon polyMultiPolygonPolygon(polyMultiPolygon mp, int idx);
int polyPointInside(polyPoint p, polyPolygon exterior, polyMultiPolygon holes);
int polyPolygonInside(polyPolygon shape, polyPolygon exterior, polyMultiPolygon holes);
polyRect polyPolygonRect(polyPolygon pp);
int polyRectIntersectsRect(polyRect r, polyRect rect);
int polyRectInsideRect(polyRect r, polyRect rect);
char *polyPolygonString(polyPolygon pp);
int polyPointIntersects(polyPoint p, polyPolygon exterior, polyMultiPolygon holes);
int polyPolygonIntersects(polyPolygon shape, polyPolygon exterior, polyMultiPolygon holes);
int polyPolygonContains(polyPolygon shape, polyPolygon exterior, polyMultiPolygon holes);
int polyLinesIntersect(polyPoint a1, polyPoint a2, polyPoint b1, polyPoint b2);

polyPoint lineCenter(polyPoint a, polyPoint b);
int polyLineIntersect(polyPoint a, polyPoint b, polyPolygon exterior, polyMultiPolygon holes);
int polyLineInside(polyPoint a, polyPoint b, polyPolygon exterior, polyMultiPolygon holes);
int lineintersects(polyPoint a, polyPoint b, polyPoint c, polyPoint d);

#if defined(__cplusplus)
}
#endif

#endif  /* POLY_H_ */

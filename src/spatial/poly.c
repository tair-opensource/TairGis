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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "zmalloc.h"
#include "poly.h"
#include "grisu3.h"

// InsideRect detects point is inside of another rect
int polyPointInsideRect(polyPoint p, polyRect r) {
	if (p.x < r.min.x || p.x > r.max.x) {
		return 0;
	}
	if (p.y < r.min.y || p.y > r.max.y) {
		return 0;
	}
	return 1;
}

polyPolygon polyPolygonFromGeomSegment(void *segment, int dims){
	polyPolygon pp;
	pp.len = (int)((uint32_t*)segment)[0];
	pp.dims = dims;
	pp.values = (double*)(((uint8_t*)segment)+4);
	return pp;
}

polyPoint polyPolygonPoint(polyPolygon pp, int idx){
	polyPoint p = {
		pp.values[idx*pp.dims+0],
		pp.values[idx*pp.dims+1],
	};
	return p;
}

polyMultiPolygon polyMultiPolygonFromGeomSegment(void *segment, int dims){
	polyMultiPolygon mp;
	mp.len = (int)((uint32_t*)segment)[0];
	mp.dims = dims;
	mp.values = (((uint8_t*)segment)+4);
	return mp;	
}

polyPolygon polyMultiPolygonPolygon(polyMultiPolygon mp, int idx){
	uint8_t *segment = mp.values;
	for (int i=0;i<idx;i++){
		segment = (segment+4+(8*mp.dims)*(*((uint32_t*)segment)));
	}
	return polyPolygonFromGeomSegment(segment, mp.dims);
}


// InsideRect detects polygon is inside of another rect
int polyPolygonInsideRect(polyPolygon pp, polyRect r){
	if (pp.len == 0){
		return 0;
	}
	for (int i=0;i<pp.len;i++){
		if (!polyPointInsideRect(polyPolygonPoint(pp, i), r)){
			return 0;
		}
	}
	return 1;
}

// Rect returns the bounding box rectangle for the polygon
polyRect polyPolygonRect(polyPolygon pp){
	polyRect bbox = {{0,0},{0,0}};
	for (int i=0;i<pp.len;i++) {
		polyPoint p = polyPolygonPoint(pp, i);
		if (i == 0) {
			bbox.min = p;
			bbox.max = p;
		} else {
			if (p.x < bbox.min.x) {
				bbox.min.x = p.x;
			} else if (p.x > bbox.max.x) {
				bbox.max.x = p.x;
			}
			if (p.y < bbox.min.y) {
				bbox.min.y = p.y;
			} else if (p.y > bbox.max.y) {
				bbox.max.y = p.y;
			}
		}
	}
	return bbox;
}

// IntersectsRect detects if two bboxes intersect.
int polyRectIntersectsRect(polyRect r, polyRect rect) {
	if (r.min.y > rect.max.y || r.max.y < rect.min.y) {
		return 0;
	}
	if (r.min.x > rect.max.x || r.max.x < rect.min.x) {
		return 0;
	}
	return 1;
}

// InsideRect detects rect is inside of another rect
int polyRectInsideRect(polyRect r, polyRect rect) {
	if (r.min.x < rect.min.x || r.max.x > rect.max.x) {
		return 0;
	}
	if (r.min.y < rect.min.y || r.max.y > rect.max.y) {
		return 0;
	}
	return 1;
}

// String returns a string representation of the polygon.
char *polyPolygonString(polyPolygon pp){
	int len = 0;
	char *str = NULL;
	str = zmalloc(10);
	if (!str){
		return NULL;
	}
	str[len++] = '{';
	for (int i=0;i<pp.len;i++){
		polyPoint p = polyPolygonPoint(pp, i);
		if (i > 0){
			str[len++] = ',';
			str[len++] = ' ';
		}
		char buf[250];
		char dx[26];
		char dy[26];
		dtoa_grisu3(p.x, dx);
		dtoa_grisu3(p.y, dy);
		sprintf(buf, "{%s, %s}", dx, dy);
		char *nstr = zrealloc(str, len+strlen(buf)+10);
		if (!nstr){
			zfree(nstr);
			return NULL;
		}
		str = nstr;
		str[len] = '\0';
		strcat(str+len, buf);
		len += strlen(buf);
	}
	str[len++] = '}';
	str[len++] = '\0';
	return str;
}

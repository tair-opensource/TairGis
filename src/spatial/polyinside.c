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
#include <stdio.h>
#include "poly.h"

static polyMultiPolygon emptyMultiPolygon = {0,0,0};

int insideshpext(polyPoint p, polyPolygon shape, int exterior) {
	// if len(shape) < 3 {
	// 	return false
	// }
	int in = 0;
	for (int i = 0; i < shape.len; i++) {
		polyPoint a = polyPolygonPoint(shape, i);
		polyPoint b = polyPolygonPoint(shape, (i+1)%shape.len);
		polyRayres res = polyRaycast(p, a, b);
		if (res == RAY_ON) {
			return exterior;
		}

		if (res == RAY_LEFT) {
			in = !in;
		}
	}
	return in;
}


// Inside returns true if point is inside of exterior and not in a hole.
// The validity of the exterior and holes must be done elsewhere and are assumed valid.
//   A valid exterior is a near-linear ring.
//   A valid hole is one that is full contained inside the exterior.
//   A valid hole may not share the same segment line as the exterior.
int polyPointInside(polyPoint p, polyPolygon exterior, polyMultiPolygon holes) {
	if (!insideshpext(p, exterior, 1)) {
		return 0;
	}
	for (int i=0;i<holes.len;i++){
		polyPolygon hole = polyMultiPolygonPolygon(holes, i);
		if (insideshpext(p, hole, 0)) {
			return 0;
		}
	}
	return 1;
}

int polyLineIntersect(polyPoint a, polyPoint b, polyPolygon exterior, polyMultiPolygon holes){
    if(polyPointInside(a,exterior,holes)||polyPointInside(b,exterior,holes))
		return 1;
	
    for(int i=0;i<exterior.len;i++){
        polyPoint exteriorP1 = polyPolygonPoint(exterior, i);
	polyPoint exteriorP2 = polyPolygonPoint(exterior, (i+1)%exterior.len);

	if(lineintersects(a,b,exteriorP1,exteriorP2))
		return 1;
    }
		
    return 0;
}

int polyLineInside(polyPoint a, polyPoint b, polyPolygon exterior, polyMultiPolygon holes){
    if(!polyPointInside(a,exterior,holes)||!polyPointInside(b,exterior,holes))
		return 0;

    int intersectNums = 0;
    for(int i=0;i<exterior.len;i++){
	polyPoint exteriorP1 = polyPolygonPoint(exterior, i);
	polyPoint exteriorP2 = polyPolygonPoint(exterior, (i+1)%exterior.len);

	if(!lineintersects(a,b,exteriorP1,exteriorP2)){
		continue;
	}
	else{
		if((polyRaycast(a,exteriorP1,exteriorP2)!=RAY_ON)&&
			(polyRaycast(b,exteriorP1,exteriorP2)!=RAY_ON)){
			intersectNums++;
		}
	}
    }

    if(intersectNums%2 == 0)
        return 1;

    return 0;
	
}

// Inside returns true if shape is inside of exterior and not in a hole.
int polyPolygonInside(polyPolygon shape, polyPolygon exterior, polyMultiPolygon holes) {
	int ok = 0;
	for (int i=0;i<shape.len;i++){
		polyPoint p = polyPolygonPoint(shape, i);
		ok = polyPointInside(p, exterior, holes);
		if (!ok) {
			return 0;
		}
	}
	ok = 1;
	for (int i=0;i<holes.len;i++){
		polyPolygon hole = polyMultiPolygonPolygon(holes, i);
		if (polyPolygonInside(hole, shape, emptyMultiPolygon)) {
			return 0;
		}
	}
	return ok;
}



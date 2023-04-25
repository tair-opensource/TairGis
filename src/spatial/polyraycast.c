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

// This implementation of the raycast algorithm test if (a point is
// to the left of a line, or on the segment line. Otherwise it is
// assumed that the point is outside of the segment line.

#include "poly.h"

const char *polyRayresString(polyRayres r){
	switch (r) {
	default:
		return "unknown";
	case RAY_OUT:
		return "out";
	case RAY_LEFT:
		return "left";
	case RAY_ON:
		return "on";
	}
}

polyRayres polyRaycast(polyPoint p, polyPoint a, polyPoint b) {
//	Algorithm 1, There may be a potential bug in it.
/*	if (a.y == b.y) {
		// A and B share the same Y plane.
		if (a.x == b.x) {
			// AB is just a point.
			if (p.x == a.x && p.y == a.y) {
				return RAY_ON;
			}
			return RAY_OUT;
		}
		// AB is a horizontal line.
		if (p.y != a.y) {
			// P is not on same Y plane as A and B.
			return RAY_OUT;
		}
		// P is on same Y plane as A and B
		if (a.x < b.x) {
			if (p.x >= a.x && p.x <= b.x) {
				return RAY_ON;
			}
			if (p.x < a.x) {
				return RAY_LEFT;
			}
		} else {
			if (p.x >= b.x && p.x <= a.x) {
				return RAY_ON;
			}
			if (p.x < b.x) {
				return RAY_LEFT;
			}
		}
		return RAY_OUT;
	}
	if (a.x == b.x) {
		// AB is a vertical line.
		if (a.y > b.y) {
			// A is above B
			if (p.y > a.y || p.y < b.y) {
				return RAY_OUT;
			}
		} else {
			// B is above A
			if (p.y > b.y || p.y < a.y) {
				return RAY_OUT;
			}
		}
		if (p.x == a.x) {
			return RAY_ON;
		}
		if (p.x < a.x) {
			return RAY_LEFT;
		}
		return RAY_OUT;
	}

	// AB is an angled line
	if (a.y > b.y) {
		// swap A and B so that A is below B.
		double tx = a.x;
		double ty = a.y;
		a.x = b.x;
		a.y = b.y;
		b.x = tx;
		b.y = ty;
	}
	if (p.y < a.y || p.y > b.y) {
		return RAY_OUT;
	}
	if (a.x < b.x) {
		if (p.x < a.x) {
			return RAY_LEFT;
		}
		if (p.x > b.x) {
			return RAY_OUT;
		}
	} else {
		if (p.x < b.x) {
			return RAY_LEFT;
		}
		if (p.x > a.x) {
			return RAY_OUT;
		}
	}
	if ((p.x == a.x && p.y == a.y) || (p.x == b.x && p.y == b.y)) {
		// P is on a vertex.
		return RAY_ON;
	}
	double v1 = (p.y - a.y) / (p.x - a.x);
	double v2 = (b.y - a.y) / (b.x - a.x);
	if (p.x != a.x) {
		if (v1-v2 == 0) {
			// P is on a segment
			return RAY_ON;
		}
		if (v1 >= v2) {
			return RAY_LEFT;
		}
		return RAY_OUT;
	} else {
		if (v2 < 0 || (v2 > 0 && p.y < a.y)) {
			return RAY_OUT;
		} else {
			return RAY_LEFT;
		}
	} 
*/

//	Algorithm 2 by W. Randolph Franklin
	if (a.x == b.x && a.y == b.y) {
		if (p.x == a.x && p.y == a.y) {
			return RAY_ON;
		}
		return RAY_OUT;
	}
	// try to check if p is on line ab and p is on segment ab.
	if (((a.x <= p.x && p.x <= b.x) || (b.x <= p.x && p.x <= a.x)) && ((a.y <= p.y && p.y <= b.y) || (b.y <= p.y && p.y <= a.y)) && (p.y - a.y) * (b.x - a.x) == (p.x - a.x) * (b.y - a.y)) {
		return RAY_ON;
	} else {
		// below tries to check:
		// min(a.y, b.y) <= p.y < max(a.y, b.y); when ab is parallel with y=p.y, it is always false;
		// assume Q is the crosspoint of line y=p.y and line ab, check if p.x < Q.x;
		if (((a.y > p.y) != (b.y > p.y)) && (p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x)) {
			return RAY_LEFT;
		} else {
			return RAY_OUT;
		}
	}
}

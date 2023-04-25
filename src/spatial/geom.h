/*
 * Copyright (c) 2016, Josh Baker <joshbaker77@gmail.com>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Redis nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GEOM_H_
#define GEOM_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include "poly.h"

typedef enum {
    GEOM_UNKNOWN            = 0,
    GEOM_POINT              = 1,
    GEOM_LINESTRING         = 2,
    GEOM_POLYGON            = 3,
    GEOM_MULTIPOINT         = 4,
    GEOM_MULTILINESTRING    = 5,
    GEOM_MULTIPOLYGON       = 6,
    GEOM_GEOMETRYCOLLECTION = 7,
} geomType;

#define GEOM_VALID_TYPE(t) ((t)>=GEOM_POINT&&(t)<=GEOM_GEOMETRYCOLLECTION)

typedef enum {
	GEOM_ERR_NONE,
	GEOM_ERR_UNSUPPORTED,
	GEOM_ERR_MEMORY,
	GEOM_ERR_INPUT,
} geomErr;

const char *geomErrText(geomErr err);

typedef enum {
    GEOM_WKT_SHOW_ZM    = 1<<0, // Show 'Z' and 'M' for 3D/4D points.
    GEOM_WKT_SHOW_EMPTY = 2<<0, // Use 'EMPTY' instead of '()'
} geomWKTEncodeOpts;

typedef enum {
    GEOM_WKT_LEAVE_OPEN = 1<<0, // Used when parsing collections.
    GEOM_WKT_REQUIRE_ZM = 1<<1, // 'Z' and 'M' are required for 3D/4D points.
} geomWKTDecodeOpts;

typedef char *geom;

typedef struct geomCoord {
    double x, y, z, m;
} geomCoord;

int geomCoordString(geomCoord c, int withZ, int withM, char *str);

typedef struct geomRect {
    geomCoord max, min;
} geomRect;

geomCoord geomRectCenter(geomRect r);
geomRect geomRectExpand(geomRect r, geomCoord c);
geomRect geomRectUnion(geomRect r1, geomRect r2);
int geomRectString(geomRect r, int withZ, int withM, char *str);
geomErr geomDecodeWKT(const char *input, geomWKTDecodeOpts opts, geom *g, int *size);
geomErr geomDecodeWKB(const void *input, size_t length, geom *g, int *size);
geomErr geomDecode(const void *input, size_t length, geomWKTDecodeOpts opts, geom *g, int *size);
void geomFree(geom g);

char *geomEncodeWKT(geom g, geomWKTEncodeOpts opts);
void geomFreeWKT(char *wkt);
char *geomEncodeJSON(geom g);
void geomFreeJSON(char *json);

geomType geomGetType(geom g);
geomCoord geomCenter(geom g);
geomRect geomBounds(geom g);

typedef struct geomIterator {
    int count;
    uint8_t *ptr;
    geom g;
    int sz;
} geomIterator;

geomErr geomGeometryCollectionIterator(geom g, geomIterator *itr);
int geomIteratorNext(geomIterator *itr);
int geomIteratorValues(geomIterator *itr, geom *g, int *sz);

geom *geomGeometryCollectionFlattenedArray(geom g, int *count);
void geomFreeFlattenedArray(geom *garr);
geom geomNewCirclePolygon(geomCoord center, double meters, int steps, int *size);
geom geomNewRectPolygon(geomRect rect, int *size);
int geomIsSimplePoint(geom g);
int geomCoordWithinRadius(geomCoord c, geomCoord center, double meters);



/* geomPolyMap is flattened representation of a geometry.
 * Each geometry is reduced down to a POINT, LINESTRING, or POLYGON.
 * The resulting polygons are stored in the 'polygons' array.
 * The number of polygons in the array is specified in 'polygonCount'.
 * The 'holes' and 'types' arrays will have 'polygonCount' number of elements.
 *
 * For example, a simple GEOM_POINT converted to a PolyMap will have 
 * one element in the 'polygons' array that contains the point data, one 
 * element in the 'holes' array that is empty, and one element in the 
 * types array that is GEOM_POINT.
 */
typedef struct geomPolyMap{
    geom g;         // first geometry.
    geomType type;  // type of the first geometry.
    int z,m;        // indicates that z or m are provided.
    int dims;       // number of dimensions per point.
    int collection; // the geometries are on the heap.
    int multipoly;  // the polygons and holes are on the heap.
    int shared;     // this structure does not need to be freed.

    // geoms
    int geomCount;  // the number of geometries.
    geom *geoms;    // one or more geometries.

    // polys
    polyPoint center;        // center point of the first geometry.
    polyRect bounds;         // minimum bounding rect of the first geometry.
    int polygonCount;        // number of polygons and holes.
    polyPolygon *polygons;   // all of the polygons belonging to the geometry.
    polyMultiPolygon *holes; // all of the holes belonging to the geometry.
    geomType *types;         // the geometry type for each polygon/holes.

    // some private vars
    polyPolygon ppoly;
    polyMultiPolygon pholes;
} geomPolyMap;

void geomFreePolyMap(geomPolyMap *m);
geomPolyMap *geomNewPolyMap(geom g);

int geomPolyMapIntersects(geomPolyMap *m1, geomPolyMap *m2);
int geomPolyMapWithin(geomPolyMap *m1, geomPolyMap *m2);

int geomPolyMapContains(geomPolyMap *m1, geomPolyMap *m2);
int geomPolyMapExIntersects(geomPolyMap *m1, geomPolyMap *m2);

#if defined(__cplusplus)
}
#endif
#endif /* GEOM_H_ */

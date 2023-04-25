/*
 * Copyright 2022 Alibaba Tair Team
 * Copyright (c) 2016, Josh Baker <joshbaker77@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
 * disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
 * following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
 * products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SPATIAL_H
#define SPATIAL_H

#include "spatial/rtree.h"
#include "spatial/geoutil.h"
#include "spatial/geom.h"
#include "spatial/hash.h"
#include "spatial/bing.h"
#include "redismodule.h"

#define FENCE_ENTER    (1<<1)
#define FENCE_EXIT     (1<<2)
#define FENCE_CROSS    (1<<3)
#define FENCE_INSIDE   (1<<4)
#define FENCE_OUTSIDE  (1<<5)
#define FENCE_KEYDEL   (1<<6)
#define FENCE_FIELDDEL (1<<7)
#define FENCE_FLUSH    (1<<8)
#define FENCE_ALL      (FENCE_ENTER|FENCE_EXIT|FENCE_CROSS|\
                       FENCE_INSIDE|FENCE_OUTSIDE|FENCE_KEYDEL|\
                       FENCE_FIELDDEL|FENCE_FIELDDEL)

#define FENCE_NOTIFY_SET 1
#define FENCE_NOTIFY_DEL 2

#define WITHIN     1
#define INTERSECTS 2
#define EX_CONTAINS 3
#define EX_INTERSECTS 4

#define RADIUS     1
#define GEOMETRY   2
#define BOUNDS     3

#define OUTPUT_COUNT    1
#define OUTPUT_FIELD    2
#define OUTPUT_WKT      3
#define OUTPUT_WKB      4
#define OUTPUT_JSON     5
#define OUTPUT_POINT    6
#define OUTPUT_BOUNDS   7
#define OUTPUT_HASH     8
#define OUTPUT_QUAD     9
#define OUTPUT_TILE    10

typedef struct fence {
    //robj *channel;
    int allfields;
    char *pattern;
    int targetType;
    geomCoord center;
    double meters;
    int searchType;
    geom g;
    int sz;
    geomPolyMap *m;
} fence;

typedef struct spatial {
    RedisModuleDict *h;        // main hash store that persists to RDB.
    rtree *tr;      // underlying spatial index.
    fence **fences; // the stored fences
    int fcap, flen; // the cap/len for fence array

    // The following fields are for mapping an idx to a key and
    // vice versa. The rtree expects that each entry has a pointer to
    // an object in memory. This should be the base address of the
    // sds key that is stored in the main hash store, that way there is
    // no extra memory overhead except a pointer. But at the moment I'm
    // 100% sure how safe it is to expect that a key in the hash will
    // not change it's base pointer address. So until further testing
    // around hash key stability we will increment an idx pointer and
    // map this value to a key, then assign this idx value to each
    // rtree entry. This allows a reverse lookup to the key. This is a
    // safe (albeit slower and higher mem usage) way to track entries.
    char *idx;     // pointer that acts as a private id for entries.
    RedisModuleDict *keyhash; // stores key -> idx
    RedisModuleDict *idxhash; // stores idx -> key
} spatial;

typedef struct resultItem {
    RedisModuleString *field;
    RedisModuleString *value;
    double distance;
} resultItem;

typedef struct searchContext {
    spatial *s;
    RedisModuleCtx *c;
    int searchType;
    int targetType;
    int fail;
    int len;
    int cap;
    resultItem *results;
    long long cursor;
    char *pattern;
    int allfields;
    int output;
    int precision;
    int nofields;
    int fence;
    int releaseg;

    // bounds
    geomRect bounds;

    // radius
    geomCoord center;
    double meters;
    double to_meters;

    // geometry
    geom g;
    int sz;
    geomPolyMap *m;

    // flags eg. WITHOUTWKT
    int flag;
    long long count;
    long long limit;

} searchContext;

typedef struct ExGisObj {
    spatial *s;
} ExGisObj;

spatial *spatialNew();
void spatialFree(spatial *s);
int spatialTypeSet(ExGisObj *o, RedisModuleString *field, RedisModuleString *val);
int spatialTypeDelete(ExGisObj *o, RedisModuleString *field, geomRect *rin, int *isEmpty);
RedisModuleString *decodeOrReply(RedisModuleCtx *ctx, const char *value);
void addGeomHashFieldToReply(RedisModuleCtx *ctx, ExGisObj *o, RedisModuleString *field);
void addGeomHashAllToReply(RedisModuleCtx *ctx, ExGisObj *o, int flag);
int searchIterator(double minX, double minY, double maxX, double maxY, void *item, void *userdata);
int sortDistanceAsc(const void *a, const void *b);
int sortDistanceDesc(const void *a, const void *b);

#endif // SPATIAL_H


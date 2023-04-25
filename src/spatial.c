/*
 * Copyright 2023 Alibaba Tair Team
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

#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "spatial.h"
#include "util.h"
#include "tairgis.h"

int matchSearch(
        geom g, geomPolyMap *targetMap,
        int targetType, int searchType,
        geomCoord center, double meters
);

spatial *spatialNew() {
    spatial *s = RedisModule_Alloc(sizeof(spatial));
    if (!s) return NULL;
    s->h = RedisModule_CreateDict(NULL);
    s->keyhash = RedisModule_CreateDict(NULL);
    s->idxhash = RedisModule_CreateDict(NULL);
    s->tr = rtreeNew();
    s->fences = NULL;
    if (!s->tr) {
        spatialFree(s);
        return NULL;
    }
    return s;
}

void redisModuleDictFree(RedisModuleDict *h) {
    if (!h) return;
    size_t keylen;
    void *data;
    RedisModuleDictIter *iter = RedisModule_DictIteratorStartC(
            h, "^", NULL, 0);
    while (RedisModule_DictNextC(iter, &keylen, &data)) {
        GisModule_FreeStringSafe(NULL, (RedisModuleString *) data);
    }
    RedisModule_FreeDict(NULL, h);
    RedisModule_DictIteratorStop(iter);
}

void spatialFree(spatial *s) {
    if (s) {
        if (s->h) redisModuleDictFree(s->h);
        if (s->keyhash) redisModuleDictFree(s->keyhash);
        if (s->idxhash) redisModuleDictFree(s->idxhash);
        if (s->tr) rtreeFree(s->tr);
        /* do not free the fence object, only the array.
         * seems there exists some mem leak */
        if (s->fences) RedisModule_Free(s->fences);
        RedisModule_Free(s);
    }
}

// get an sds based on the key.
// return value must be freed by the caller.
static RedisModuleString * hashTypeGetRedisModuleString(RedisModuleDict *o, RedisModuleString *key) {
    int nokey = 0;
    RedisModuleString *vstr = RedisModule_DictGet(o, key, &nokey);
    if (nokey == 1 || !vstr) return NULL;
    return vstr;
}

// returns a raw pointer to the value
static const void *hashTypeGetRaw(RedisModuleDict *o, RedisModuleString *key) {
    int nokey = 0;
    RedisModuleString *vstr = RedisModule_DictGet(o, key, &nokey);
    if (nokey == 1 || !vstr) return NULL;
    return RedisModule_StringPtrLen(vstr, NULL);
}

int spatialTypeSet(ExGisObj *o, RedisModuleString *field, RedisModuleString *val) {
    geom g;
    geomRect r;
    RedisModuleString *sidx;
    uint64_t nidx;
    spatial *s = o->s;

    g = (geom) RedisModule_StringPtrLen(val, NULL);
    r = geomBounds(g);

    /* try to delete the former existing data for 'field'
     * also remove what's exiting in rtree */
    spatialTypeDelete(o, field, &r, NULL);

    /* create a new idx / field entry */
    s->idx++;
    nidx = (uint64_t) s->idx;
    sidx = RedisModule_CreateString(NULL, (const char *) &nidx, 8);

    GisModule_DictInsertOrUpdate(s->idxhash, sidx, field);
    GisModule_DictInsertOrUpdate(s->keyhash, field, sidx);
    GisModule_FreeStringSafe(NULL, sidx);

    GisModule_DictInsertOrUpdate(s->h, field, val);

    /* update the rtree */
    rtreeInsert(s->tr, r.min.x, r.min.y, r.max.x, r.max.y, s->idx);

    return 1;
}

RedisModuleString *decodeOrReply(RedisModuleCtx *ctx, const char *value) {
    geom g = NULL;
    int sz = 0;
    geomErr err = geomDecode(value, strlen(value), 0, &g, &sz);
    if (err != GEOM_ERR_NONE) {
        RedisModule_ReplyWithError(ctx, "ERR invalid geometry");
        return NULL;
    }
    RedisModuleString *gvalue = RedisModule_CreateString(NULL, g, (size_t) sz);
    geomFree(g);
    return gvalue;
}

int matchSearch(
        geom g, geomPolyMap *targetMap,
        int targetType, int searchType,
        geomCoord center, double meters
) {
    int match = 0;
    if (geomIsSimplePoint(g) && targetType == RADIUS) {
        match = geomCoordWithinRadius(geomCenter(g), center, meters);
    } else {
        geomPolyMap *m = geomNewPolyMap(g);
        if (!m) {
            return 0;
        }
        if (searchType == WITHIN) {
            match = geomPolyMapWithin(m, targetMap);
        } else if (searchType == INTERSECTS) {
            match = geomPolyMapIntersects(m, targetMap);
        } else if(searchType == EX_CONTAINS) {
            match = geomPolyMapContains(m,targetMap);
        } else if(searchType == EX_INTERSECTS) {
            match = geomPolyMapExIntersects(m,targetMap);
        }
        geomFreePolyMap(m);
    }
    return match;
}

int RedisModule_StringPtrIsEmpty(const char *str) {
    return strcmp(str, "(NULL string reply referenced in module)");
}

int spatialTypeDelete(ExGisObj *o, RedisModuleString *field, geomRect *rin, int *isEmpty) {
    geomRect r;
    RedisModuleString *sidx;
    char *idx;
    int res;
    spatial *s = o->s;
    const char *cstr = NULL;
    size_t len = 0;

    /* get the idx */
    sidx = hashTypeGetRedisModuleString(s->keyhash, field);
    cstr = RedisModule_StringPtrLen(sidx, &len);
    if (!RedisModule_StringPtrIsEmpty(cstr) || len != 8) return 0;
    idx = (char *) (*((uint64_t *) cstr));

    geom g = NULL;
    if (rin) {
        r = *rin;
    } else {
        const void *raw = hashTypeGetRaw(s->h, field);
        if (!raw) return 0;
        g = (geom) raw;
        r = geomBounds(g);
    }

    rtreeRemove(s->tr, r.min.x, r.min.y, r.max.x, r.max.y, idx);

    RedisModuleString *o1 = NULL, *o2 = NULL, *o3 = NULL;
    res = RedisModule_DictDel(s->h, field, &o1) || RedisModule_DictDel(s->idxhash, sidx, &o2) ||
          RedisModule_DictDel(s->keyhash, field, &o3);

    GisModule_FreeStringSafe(NULL, o1);
    GisModule_FreeStringSafe(NULL, o2);
    GisModule_FreeStringSafe(NULL, o3);

    if (RedisModule_DictSize(s->keyhash) == 0 && isEmpty) {
        *isEmpty = 1;
    }

    return res == REDISMODULE_OK ? 1 : 0;
}

/* Importing some stuff from t_hash.c but these should exist in server.h */
void addGeomReplyBulkCBuffer(RedisModuleCtx *ctx, const void *p) {
    char *wkt = geomEncodeWKT((geom) p, 0);
    if (!wkt) {
        RedisModule_ReplyWithError(ctx, "ERR failed to encode wkt");
        return;
    }
    RedisModule_ReplyWithStringBuffer(ctx, wkt, strlen(wkt));
    geomFreeWKT(wkt);
}

/* These are direct copies from t_hash.c because they're defined as static and
 * I didn't want to change the source file. */
void addGeomHashFieldToReply(RedisModuleCtx *ctx, ExGisObj *o, RedisModuleString *field) {
    if (o == NULL) {
        RedisModule_ReplyWithNull(ctx);
        return;
    }
    RedisModuleString *vstr = NULL;
    int nokey = 0;

    vstr = RedisModule_DictGet(o->s->h, field, &nokey);
    if (nokey == 1 || !vstr) {
        RedisModule_ReplyWithNull(ctx);
        return;
    }

    addGeomReplyBulkCBuffer(ctx, RedisModule_StringPtrLen(vstr, NULL));
}

void addGeomHashAllToReply(RedisModuleCtx *ctx, ExGisObj *o, int flag) {
    if (o == NULL) {
        RedisModule_ReplyWithNull(ctx);
        return;
    }

    long size = (long) RedisModule_DictSize(o->s->h);
    RedisModule_ReplyWithArray(ctx, flag & GIS_WITHVALUE ? size * 2 : size);

    void *field, *val;
    RedisModuleDictIter *iter = RedisModule_DictIteratorStartC(
            o->s->h, "^", NULL, 0);
    while ((field = RedisModule_DictNext(NULL, iter, &val)) != NULL) {
        RedisModule_ReplyWithString(ctx, field);
        if (flag & GIS_WITHVALUE) {
            addGeomReplyBulkCBuffer(ctx, RedisModule_StringPtrLen(val, NULL));
        }
        GisModule_FreeStringSafe(NULL, field);
    }
    RedisModule_DictIteratorStop(iter);
}

/* Glob-style pattern matching. */
// Move from redis src/util.c
static int stringmatchlen(const char *pattern, int patternLen,
                          const char *string, int stringLen, int nocase)
{
    while(patternLen && stringLen) {
        switch(pattern[0]) {
            case '*':
                while (pattern[1] == '*') {
                    pattern++;
                    patternLen--;
                }
                if (patternLen == 1)
                    return 1; /* match */
                while(stringLen) {
                    if (stringmatchlen(pattern+1, patternLen-1,
                                       string, stringLen, nocase))
                        return 1; /* match */
                    string++;
                    stringLen--;
                }
                return 0; /* no match */
                break;
            case '?':
                if (stringLen == 0)
                    return 0; /* no match */
                string++;
                stringLen--;
                break;
            case '[':
            {
                int not, match;

                pattern++;
                patternLen--;
                not = pattern[0] == '^';
                if (not) {
                    pattern++;
                    patternLen--;
                }
                match = 0;
                while(1) {
                    if (pattern[0] == '\\' && patternLen >= 2) {
                        pattern++;
                        patternLen--;
                        if (pattern[0] == string[0])
                            match = 1;
                    } else if (pattern[0] == ']') {
                        break;
                    } else if (patternLen == 0) {
                        pattern--;
                        patternLen++;
                        break;
                    } else if (pattern[1] == '-' && patternLen >= 3) {
                        int start = pattern[0];
                        int end = pattern[2];
                        int c = string[0];
                        if (start > end) {
                            int t = start;
                            start = end;
                            end = t;
                        }
                        if (nocase) {
                            start = tolower(start);
                            end = tolower(end);
                            c = tolower(c);
                        }
                        pattern += 2;
                        patternLen -= 2;
                        if (c >= start && c <= end)
                            match = 1;
                    } else {
                        if (!nocase) {
                            if (pattern[0] == string[0])
                                match = 1;
                        } else {
                            if (tolower((int)pattern[0]) == tolower((int)string[0]))
                                match = 1;
                        }
                    }
                    pattern++;
                    patternLen--;
                }
                if (not)
                    match = !match;
                if (!match)
                    return 0; /* no match */
                string++;
                stringLen--;
                break;
            }
            case '\\':
                if (patternLen >= 2) {
                    pattern++;
                    patternLen--;
                }
                /* fall through */
            default:
                if (!nocase) {
                    if (pattern[0] != string[0])
                        return 0; /* no match */
                } else {
                    if (tolower((int)pattern[0]) != tolower((int)string[0]))
                        return 0; /* no match */
                }
                string++;
                stringLen--;
                break;
        }
        pattern++;
        patternLen--;
        if (stringLen == 0) {
            while(*pattern == '*') {
                pattern++;
                patternLen--;
            }
            break;
        }
    }
    if (patternLen == 0 && stringLen == 0)
        return 1;
    return 0;
}

int searchIterator(double minX, double minY, double maxX, double maxY, void *item, void *userdata) {
    (void)(minX);(void)(minY);(void)(maxX);(void)(maxY); // unused vars.

    searchContext *ctx = userdata;

    /* if limit reach, just return */
    if ((ctx->limit != 0) && (ctx->len >= ctx->limit)) {
        return 1;
    }

    /* retrieve the field */
    int nokey = 0;
    uint64_t nidx = (uint64_t)item;
    RedisModuleString *sidx = RedisModule_CreateString(NULL, (const char *) &nidx, 8);
    RedisModuleString *field = RedisModule_DictGet(ctx->s->idxhash, sidx, &nokey);
    GisModule_FreeStringSafe(NULL, sidx);
    if (nokey == 1) {
        return 1;
    }

    const char *fieldStr = NULL, *valueStr = NULL;
    size_t filedLen, valueLen;
    fieldStr = RedisModule_StringPtrLen(field, &filedLen);

    if (!(ctx->allfields ||
            stringmatchlen(ctx->pattern, (int) strlen(ctx->pattern), fieldStr, (int) filedLen, 0))) {
        return 1;
    }

    // retrieve the geom
    RedisModuleString *value = RedisModule_DictGet(ctx->s->h, field, &nokey);
    valueStr = RedisModule_StringPtrLen(value, &valueLen);
    if (nokey == 1){
        return 1;
    }

    geom g = (geom)valueStr;

    int match = matchSearch(g, ctx->m, ctx->targetType, ctx->searchType, ctx->center, ctx->meters);
    if (!match){
        return 1;
    }
    // append item
    if (ctx->len == ctx->cap) {
        int ncap = ctx->cap;
        if (ncap == 0){
            ncap = 1;
        } else {
            ncap *= 2;
        }
        resultItem *nresults = RedisModule_Realloc(ctx->results, ncap*sizeof(resultItem));
        if (!nresults){
            RedisModule_ReplyWithError(ctx->c, "ERR out of memory");
            ctx->fail = 1;
            return 0;
        }
        ctx->results = nresults;
        ctx->cap = ncap;
    }

    ctx->results[ctx->len].field = field;
    ctx->results[ctx->len].value = value;
    ctx->len++;

    return 1;
}

int sortDistanceAsc(const void *a, const void *b) {
    resultItem *da = (resultItem *)a, *db = (resultItem *)b;
    if (da->distance > db->distance) {
        return 1;
    } else if (fabs(da->distance - db->distance) < 0.0000001) {
        return 0;
    } else {
        return -1;
    }
}

int sortDistanceDesc(const void *a, const void *b) {
    return -sortDistanceAsc(a, b);
}

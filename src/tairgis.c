/*
 * Copyright 2023 Alibaba Tair Team
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "redismodule.h"
#include "spatial/rtree.h"
#include "spatial/geom.h"
#include "spatial.h"
#include "util.h"
#include "tairgis.h"

#define EXGIS_ENC_VER 0
static RedisModuleType *ExGisType;

int parseGisFlags(RedisModuleCtx *redisCtx, int start, RedisModuleString **argv, int argc, searchContext *ctx,
                  int *externflag) {
    for (int i = start; i < argc; ++i) {
        const char *field = RedisModule_StringPtrLen(argv[i], NULL);
        // GEOM
        if (!strcasecmp(field, "RADIUS")) {
            if (i >= argc - 4) {
                RedisModule_ReplyWithError(redisCtx, "ERR need longitude, latitude, meters, unit");
                goto fail;
            }

            if (GisModule_GetDoubleFromObjectOrReply(redisCtx, argv[i + 1], &ctx->center.x,
                                                       "ERR need numeric longitude") != REDISMODULE_OK)
                goto fail;
            if (GisModule_GetDoubleFromObjectOrReply(redisCtx, argv[i + 2], &ctx->center.y,
                                                       "ERR need numeric latitude") != REDISMODULE_OK)
                goto fail;
            if (GisModule_GetDoubleFromObjectOrReply(redisCtx, argv[i + 3], &ctx->meters,
                                                       "ERR need numeric meters") != REDISMODULE_OK)
                goto fail;

            if (GisModule_ExtractUnitOrReply(redisCtx, argv[i + 4], &ctx->to_meters) != REDISMODULE_OK) {
                goto fail;
            }

            if (ctx->center.x < -180 || ctx->center.x > 180 || ctx->center.y < -90 || ctx->center.y > 90) {
                RedisModule_ReplyWithError(redisCtx, "ERR invalid longitude/latitude pair");
                goto fail;
            }
            ctx->targetType = RADIUS;
            ctx->meters *= ctx->to_meters;
            ctx->bounds = geoutilBoundsFromLatLon(ctx->center.y, ctx->center.x, ctx->meters);
            ctx->g = geomNewCirclePolygon(ctx->center, ctx->meters, 12, &ctx->sz);
            i += 4;
        } else if (!strcasecmp(field, "GEOM")) {
            if (i == argc - 1) {
                RedisModule_ReplyWithError(redisCtx, "ERR need geometry");
                goto fail;
            }
            geom g = NULL;
            int sz = 0;
            const char *geomStr = RedisModule_StringPtrLen(argv[i + 1], NULL);
            geomErr err = geomDecode(geomStr, strlen(geomStr), 0, &g, &sz);
            if (err != GEOM_ERR_NONE) {
                RedisModule_ReplyWithError(redisCtx, "ERR invalid geometry");
                goto fail;
            }

            ctx->g = g;
            ctx->sz = sz;
            ctx->targetType = GEOMETRY;
            ctx->bounds = geomBounds(ctx->g);
            i += 1;
        } else if (!strcasecmp(field, "MEMBER")) {
            if (i >= argc - 3) {
                RedisModule_ReplyWithError(redisCtx, "ERR need member, meters, unit");
                goto fail;
            }

            int nokey = 0;
            size_t memberLen = 0;
            RedisModuleString *member = NULL;
            const char *memberStr = NULL;

            member = RedisModule_DictGet(ctx->s->h, argv[i + 1], &nokey);
            if (nokey == 1 || !member) {
                RedisModule_ReplyWithError(redisCtx, "ERR member not found");
                goto fail;
            }
            memberStr = RedisModule_StringPtrLen(member, &memberLen);
            if (!geomIsSimplePoint((geom) memberStr)) {
                RedisModule_ReplyWithError(redisCtx, "ERR member must be point");
                goto fail;
            }

            if (GisModule_GetDoubleFromObjectOrReply(redisCtx, argv[i + 2], &ctx->meters,
                                                       "ERR need numeric meters") != REDISMODULE_OK)
                goto fail;

            if (GisModule_ExtractUnitOrReply(redisCtx, argv[i + 3], &ctx->to_meters) != REDISMODULE_OK) {
                goto fail;
            }
            ctx->meters *= ctx->to_meters;
            ctx->center = geomCenter((geom) memberStr);
            ctx->targetType = RADIUS;
            ctx->bounds = geoutilBoundsFromLatLon(ctx->center.y, ctx->center.x, ctx->meters);
            ctx->g = geomNewCirclePolygon(ctx->center, ctx->meters, 12, &ctx->sz);
            i += 3;
        }

        // Option
        else if (!strcasecmp(field, "WITHVALUE")) {
            if (ctx) ctx->flag |= GIS_WITHVALUE;
            if (externflag) *externflag |= GIS_WITHVALUE;
        } else if (!strcasecmp(field, "WITHDIST")) {
            if (ctx) ctx->flag |= GIS_WITHDIST;
            if (externflag) *externflag |= GIS_WITHDIST;
        } else if (!strcasecmp(field, "WITHOUTVALUE")) {
            if (ctx) ctx->flag &= ~GIS_WITHVALUE;
            if (externflag) *externflag &= ~GIS_WITHVALUE;
        } else if (!strcasecmp(field, "WITHOUTWKT")) {
            if (ctx) ctx->flag &= ~GIS_WITHVALUE;
            if (externflag) *externflag &= ~GIS_WITHVALUE;
        } else if (!strcasecmp(field, "COUNT")) {
            if (i == argc - 1) {
                RedisModule_ReplyWithError(redisCtx, "ERR count need number");
                goto fail;
            }
            if (RedisModule_StringToLongLong(argv[i + 1], &ctx->count) != REDISMODULE_OK) {
                RedisModule_ReplyWithError(redisCtx, "ERR count must be number");
                goto fail;
            }

            if (ctx->count <= 0) {
                RedisModule_ReplyWithError(redisCtx, "ERR count must be > 0");
                goto fail;
            }
            i += 1;
        } else if (!strcasecmp(field, "LIMIT")) {
            if (i == argc - 1) {
                RedisModule_ReplyWithError(redisCtx, "ERR limit need number");
                goto fail;
            }
            if (RedisModule_StringToLongLong(argv[i + 1], &ctx->limit) != REDISMODULE_OK) {
                RedisModule_ReplyWithError(redisCtx, "ERR limit must be number");
                goto fail;
            }

            if (ctx->limit <= 0) {
                RedisModule_ReplyWithError(redisCtx, "ERR limit must be > 0");
                goto fail;
            }
            i += 1;
        } else if (!strcasecmp(field, "ASC")) {
            if (ctx && !(ctx->flag & GIS_SORT_DESC)) ctx->flag |= GIS_SORT_ASC;
            if (externflag && !(*externflag & GIS_SORT_DESC)) ctx->flag |= GIS_SORT_ASC;
        } else if (!strcasecmp(field, "DESC")) {
            if (ctx && !(ctx->flag & GIS_SORT_ASC)) ctx->flag |= GIS_SORT_DESC;
            if (externflag && !(*externflag & GIS_SORT_ASC)) ctx->flag |= GIS_SORT_DESC;
        }
    }
    return REDISMODULE_OK;
fail:
    if (ctx) {
        if (ctx->g && ctx->releaseg) {
            geomFree(ctx->g);
        }
        if (ctx->m) {
            geomFreePolyMap(ctx->m);
        }
        if (ctx->results) {
            RedisModule_Free(ctx->results);
        }
    }
    return REDISMODULE_ERR;
}

ExGisObj *createExGisTypeObject() {
    ExGisObj *o = RedisModule_Alloc(sizeof(ExGisObj));
    o->s = spatialNew();
    return o;
}

void releaseExGisTypeObject(ExGisObj *o) {
    if (o->s) {
        spatialFree(o->s);
    }
    RedisModule_Free(o);
}

/* ========================== Command  func =============================*/

int ExGisAdd_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if ((argc % 2) == 1 || argc < 4) {
        RedisModule_WrongArity(ctx);
        return REDISMODULE_ERR;
    }
    RedisModule_AutoMemory(ctx);

    int i, type, created = 0;
    ExGisObj *ex_gis_obj = NULL;

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    type = RedisModule_KeyType(key);
    if (REDISMODULE_KEYTYPE_EMPTY == type) {
        ex_gis_obj = createExGisTypeObject();
        RedisModule_ModuleTypeSetValue(key, ExGisType, ex_gis_obj);
    } else {
        if (RedisModule_ModuleTypeGetType(key) != ExGisType) {
            RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
            return REDISMODULE_ERR;
        }
        ex_gis_obj = RedisModule_ModuleTypeGetValue(key);
    }

    for (i = 2; i < argc; i += 2) {
        RedisModuleString *value;
        if ((value = decodeOrReply(ctx, RedisModule_StringPtrLen(argv[i + 1], NULL))) == NULL) return REDISMODULE_ERR;
        created += spatialTypeSet(ex_gis_obj, argv[i], value);
        GisModule_FreeStringSafe(NULL, value);
    }

    RedisModule_ReplyWithLongLong(ctx, created);

    RedisModule_ReplicateVerbatim(ctx);
    return REDISMODULE_OK;
}

int ExGisGet_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 3) {
        RedisModule_WrongArity(ctx);
        return REDISMODULE_ERR;
    }
    RedisModule_AutoMemory(ctx);

    int type = 0;
    ExGisObj *ex_gis_obj = NULL;

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    type = RedisModule_KeyType(key);
    if (REDISMODULE_KEYTYPE_EMPTY == type) {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    } else {
        if (RedisModule_ModuleTypeGetType(key) != ExGisType) {
            RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
            return REDISMODULE_ERR;
        }
        ex_gis_obj = RedisModule_ModuleTypeGetValue(key);
    }

    addGeomHashFieldToReply(ctx, ex_gis_obj, argv[2]);

    return REDISMODULE_OK;
}

int ExGisDel_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 3) {
        RedisModule_WrongArity(ctx);
        return REDISMODULE_ERR;
    }
    RedisModule_AutoMemory(ctx);

    int type = 0, nokey = 0, sz = 0, isEmpty = 0;
    RedisModuleString *vstr = NULL;
    const char *cstr = NULL;
    size_t len = 0;
    ExGisObj *ex_gis_obj = NULL;
    geom g = NULL;

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    type = RedisModule_KeyType(key);
    if (REDISMODULE_KEYTYPE_EMPTY == type) {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    } else {
        if (RedisModule_ModuleTypeGetType(key) != ExGisType) {
            RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
            return REDISMODULE_ERR;
        }
        ex_gis_obj = RedisModule_ModuleTypeGetValue(key);
    }

    vstr = RedisModule_DictGet(ex_gis_obj->s->h, argv[2], &nokey);
    cstr = RedisModule_StringPtrLen(vstr, &len);
    if (nokey == 1 || !vstr) {
        goto null;
    }

    geomErr err = geomDecode(cstr, len, 0, &g, &sz);
    if (err != GEOM_ERR_NONE) {
        RedisModule_ReplyWithError(ctx, "ERR invalid geometry");
        goto error;
    }

    geomRect bounds = geomBounds(g);
    int succ = spatialTypeDelete(ex_gis_obj, argv[2], &bounds, &isEmpty);
    if (!succ) {
        RedisModule_ReplyWithError(ctx, "ERR delete geo fail!");
        goto error;
    }
    if (isEmpty) {
        RedisModule_DeleteKey(key);
    }

    geomFree(g);
    RedisModule_ReplicateVerbatim(ctx);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;

null:
    RedisModule_ReplyWithNull(ctx);
    return REDISMODULE_OK;

error:
    geomFree(g);
    return REDISMODULE_ERR;
}

static int exgsearchInner(RedisModuleCtx *redisCtx, RedisModuleString **argv, int argc, int searchtype){
    if (argc < 3) {
        RedisModule_WrongArity(redisCtx);
        return REDISMODULE_ERR;
    }
    RedisModule_AutoMemory(redisCtx);

    int type = 0;
    ExGisObj *ex_gis_obj = NULL;

    RedisModuleKey *key = RedisModule_OpenKey(redisCtx, argv[1], REDISMODULE_READ);
    type = RedisModule_KeyType(key);
    if (REDISMODULE_KEYTYPE_EMPTY == type) {
        // EMPTYMULTIBULK
        RedisModule_ReplyWithArray(redisCtx, 0);
        return REDISMODULE_OK;
    } else {
        if (RedisModule_ModuleTypeGetType(key) != ExGisType) {
            RedisModule_ReplyWithError(redisCtx, REDISMODULE_ERRORMSG_WRONGTYPE);
            return REDISMODULE_ERR;
        }
        ex_gis_obj = RedisModule_ModuleTypeGetValue(key);
    }

    searchContext ctx;
    memset(&ctx, 0, sizeof(searchContext));
    ctx.c = redisCtx;
    ctx.releaseg = 1;
    ctx.searchType = searchtype;
    ctx.cursor = -1;
    ctx.allfields = 1;
    ctx.output = OUTPUT_WKT;
    ctx.s = ex_gis_obj->s;
    ctx.flag |= GIS_WITHVALUE;
    ctx.to_meters = 1;

    if (parseGisFlags(redisCtx, 2, argv, argc, &ctx, NULL) != REDISMODULE_OK) {
        return REDISMODULE_ERR;
    }

    // In order to be compatible with the previous logic
    // default geom is geometry
    if (!ctx.g) {
        geom g = NULL;
        int sz = 0;
        const char *geomStr = RedisModule_StringPtrLen(argv[2], NULL);
        geomErr err = geomDecode(geomStr, strlen(geomStr), 0, &g, &sz);
        if (err != GEOM_ERR_NONE) {
            RedisModule_ReplyWithError(redisCtx, "ERR invalid geometry");
            return REDISMODULE_ERR;
        }

        ctx.g = g;
        ctx.sz = sz;
        ctx.targetType = GEOMETRY;
        ctx.bounds = geomBounds(ctx.g);
    }

    if (ctx.g && !ctx.fence) {
        ctx.m = geomNewPolyMap(ctx.g);
        if (!ctx.m) {
            RedisModule_ReplyWithError(redisCtx, "ERR poly map failure");
            goto done;
        }
    }

    rtreeSearch(ctx.s->tr, ctx.bounds.min.x, ctx.bounds.min.y, ctx.bounds.max.x, ctx.bounds.max.y, searchIterator,
                &ctx);

    if (!ctx.fail) {
        long option_length = 1;

        if (ctx.flag & GIS_WITHVALUE) {
            option_length++;
        }

        if (ctx.flag & GIS_WITHDIST) {
            option_length++;
        }

        // SORT or COUNT
        /* COUNT without ordering does not make much sense, force ASC
         * ordering if COUNT was specified but no sorting was requested. */
        if (ctx.count != 0 && !(ctx.flag & GIS_SORT_ASC) && !(ctx.flag & GIS_SORT_DESC)) {
            ctx.flag |= GIS_SORT_ASC;
        }
        if ((ctx.flag & GIS_SORT_ASC) || (ctx.flag & GIS_SORT_DESC) || (ctx.flag & GIS_WITHDIST)) {
            for (int i = 0; i < ctx.len; i++) {
                geomCoord c = geomCenter((geom) RedisModule_StringPtrLen(ctx.results[i].value, NULL));
                double distance = geoutilDistance(c.y, c.x, ctx.center.y, ctx.center.x);
                ctx.results[i].distance = distance / ctx.to_meters;
            }

            if (ctx.flag & GIS_SORT_ASC) {
                qsort(ctx.results, ctx.len, sizeof(resultItem), sortDistanceAsc);
            } else if (ctx.flag & GIS_SORT_DESC) {
                qsort(ctx.results, ctx.len, sizeof(resultItem), sortDistanceDesc);
            }
        }

        int returned_items = (ctx.count == 0 || ctx.len < ctx.count) ?
                              ctx.len : ctx.count;

        RedisModule_ReplyWithArray(redisCtx, 2);
        RedisModule_ReplyWithLongLong(redisCtx, returned_items);
        RedisModule_ReplyWithArray(redisCtx, returned_items * option_length);
        for (int i = 0; i < returned_items; i++) {
            RedisModule_ReplyWithString(redisCtx, ctx.results[i].field);

            if (ctx.flag & GIS_WITHVALUE) {
                char *wkt = geomEncodeWKT((geom) RedisModule_StringPtrLen(ctx.results[i].value, NULL), 0);
                assert(wkt);
                RedisModule_ReplyWithStringBuffer(redisCtx, wkt, strlen(wkt));
                geomFreeWKT(wkt);
            }

            if (ctx.flag & GIS_WITHDIST) {
                GisModule_AddReplyDistance(redisCtx, ctx.results[i].distance);
            }
        }
    }

done:
    if (ctx.g && ctx.releaseg) {
        geomFree(ctx.g);
    }
    if (ctx.m) {
        geomFreePolyMap(ctx.m);
    }
    if (ctx.results) {
        RedisModule_Free(ctx.results);
    }
    return REDISMODULE_OK;
}


int ExGisSearch_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return exgsearchInner(ctx, argv, argc, INTERSECTS);
}

int ExGisWithIn_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return exgsearchInner(ctx, argv, argc, WITHIN);
}

int ExGisContains_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return exgsearchInner(ctx, argv, argc, EX_CONTAINS);
}

int ExGisIntersects_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return exgsearchInner(ctx, argv, argc, EX_INTERSECTS);
}

int ExGisGetAll_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 2) {
        RedisModule_WrongArity(ctx);
        return REDISMODULE_ERR;
    }
    RedisModule_AutoMemory(ctx);

    int type = 0;
    ExGisObj *ex_gis_obj = NULL;

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    type = RedisModule_KeyType(key);
    if (REDISMODULE_KEYTYPE_EMPTY == type) {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    } else {
        if (RedisModule_ModuleTypeGetType(key) != ExGisType) {
            RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
            return REDISMODULE_ERR;
        }
        ex_gis_obj = RedisModule_ModuleTypeGetValue(key);
    }

    int flag = GIS_WITHVALUE;
    if (parseGisFlags(ctx, 2, argv, argc, NULL, &flag) != REDISMODULE_OK) {
        return REDISMODULE_ERR;
    }

    addGeomHashAllToReply(ctx, ex_gis_obj, flag);
    return REDISMODULE_OK;
}

/* ========================== "exgistype" type methods ======================= */

void *ExGisTypeRdbLoad(RedisModuleIO *rdb, int encver) {
    if (encver != EXGIS_ENC_VER) {
        return NULL;
    }

    ExGisObj *ex_gis_obj = createExGisTypeObject();
    uint64_t size = RedisModule_LoadUnsigned(rdb);

    RedisModuleString *field, *value;
    while (size--) {
        field = RedisModule_LoadString(rdb);
        value = RedisModule_LoadString(rdb);
        spatialTypeSet(ex_gis_obj, field, value);

        GisModule_FreeStringSafe(NULL, field);
        GisModule_FreeStringSafe(NULL, value);
    }

    return ex_gis_obj;
}

void ExGisTypeRdbSave(RedisModuleIO *rdb, void *value) {
    ExGisObj *o = value;
    RedisModuleDict *h = o->s->h;
    uint64_t size = RedisModule_DictSize(h);
    RedisModule_SaveUnsigned(rdb, size);

    void *field, *val;
    RedisModuleDictIter *iter = RedisModule_DictIteratorStartC(
            h, "^", NULL, 0);
    while ((field = RedisModule_DictNext(NULL, iter, &val)) != NULL) {
        RedisModule_SaveString(rdb, field);
        RedisModule_SaveString(rdb, val);

        GisModule_FreeStringSafe(NULL, field);
    }
    RedisModule_DictIteratorStop(iter);
}

void ExGisTypeAofRewrite(RedisModuleIO *aof, RedisModuleString *key, void *value) {
    ExGisObj *o = value;
    RedisModuleDict *h = o->s->h;

    void *field, *val;
    RedisModuleDictIter *iter = RedisModule_DictIteratorStartC(
            h, "^", NULL, 0);
    while ((field = RedisModule_DictNext(NULL, iter, &val)) != NULL) {
        char *wkt = geomEncodeWKT((geom) RedisModule_StringPtrLen(val, NULL), 0);
        RedisModule_EmitAOF(aof, "GIS.ADD", "scc", key, RedisModule_StringPtrLen(field, NULL), wkt);

        RedisModule_Free(wkt);
        GisModule_FreeStringSafe(NULL, field);
    }
    RedisModule_DictIteratorStop(iter);
}

size_t ExGisTypeMemUsage(const void *value) {
    // TODO
    REDISMODULE_NOT_USED(value);
    return 0;
}

void ExGisTypeFree(void *value) {
    releaseExGisTypeObject(value);
}

void ExGisTypeDigest(RedisModuleDigest *md, void *value) {
    //TODO
    REDISMODULE_NOT_USED(md);
    REDISMODULE_NOT_USED(value);
}

size_t ExGisTypeFreeEffort(RedisModuleString *key, const void *value) {
    REDISMODULE_NOT_USED(key);
    ExGisObj *ex_gis_obj = (ExGisObj*)(value);
    return RedisModule_DictSize(ex_gis_obj->s->keyhash);
}

int Module_CreateCommands(RedisModuleCtx *ctx) {

#define CREATE_CMD(name, tgt, attr)                                                                \
    do {                                                                                           \
        if (RedisModule_CreateCommand(ctx, name, tgt, attr, 1, 1, 1) != REDISMODULE_OK) {          \
            return REDISMODULE_ERR;                                                                \
        }                                                                                          \
    } while (0);
#define CREATE_WRCMD(name, tgt) CREATE_CMD(name, tgt, "write deny-oom")
#define CREATE_ROCMD(name, tgt) CREATE_CMD(name, tgt, "readonly fast")

    CREATE_WRCMD("gis.add", ExGisAdd_RedisCommand)
    CREATE_ROCMD("gis.get", ExGisGet_RedisCommand)
    CREATE_WRCMD("gis.del", ExGisDel_RedisCommand)
    CREATE_ROCMD("gis.search", ExGisSearch_RedisCommand)
    CREATE_ROCMD("gis.contains", ExGisContains_RedisCommand)
    CREATE_ROCMD("gis.intersects", ExGisIntersects_RedisCommand)
    CREATE_ROCMD("gis.getall", ExGisGetAll_RedisCommand)
    CREATE_ROCMD("gis.within", ExGisWithIn_RedisCommand)

    return REDISMODULE_OK;
}

/* This function must be present on each Redis module. It is used in order to
 * register the commands into the Redis server. */
int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    REDISMODULE_NOT_USED(argv);
    REDISMODULE_NOT_USED(argc);

    if (RedisModule_Init(ctx,"exgistype",1,REDISMODULE_APIVER_1)
        == REDISMODULE_ERR) return REDISMODULE_ERR;

    RedisModuleTypeMethods tm = {
            .version = REDISMODULE_TYPE_METHOD_VERSION,
            .rdb_load = ExGisTypeRdbLoad,
            .rdb_save = ExGisTypeRdbSave,
            .aof_rewrite = ExGisTypeAofRewrite,
            .mem_usage = ExGisTypeMemUsage,
            .free = ExGisTypeFree,
            .digest = ExGisTypeDigest,
            .free_effort = ExGisTypeFreeEffort,
    };

    ExGisType = RedisModule_CreateDataType(ctx,"exgistype",0,&tm);
    if (ExGisType == NULL) return REDISMODULE_ERR;

    if (REDISMODULE_ERR == Module_CreateCommands(ctx)) return REDISMODULE_ERR;

    return REDISMODULE_OK;
}

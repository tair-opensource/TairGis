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

#ifndef UTIL_H
#define UTIL_H

#include <limits.h>
#include <sys/types.h>
#include <stdint.h>
#include "redismodule.h"

uint32_t GisModule_digits10(uint64_t v);
int GisModule_ll2string(char *dst, size_t dstlen, long long svalue);
int GisModule_DictInsertOrUpdate(RedisModuleDict *d, RedisModuleString *field, RedisModuleString *value);
void GisModule_FreeStringSafe(RedisModuleCtx *ctx, RedisModuleString *str);
int GisModule_GetDoubleFromObjectOrReply(RedisModuleCtx *ctx, RedisModuleString *s, double *target, const char *msg);
int GisModule_ExtractUnitOrReply(RedisModuleCtx *ctx, RedisModuleString *unit, double *to_meters);
void GisModule_AddReplyDistance(RedisModuleCtx *ctx, double d);

#endif // UTIL_H

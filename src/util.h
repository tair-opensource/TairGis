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

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

#include "util.h"
#include "redismodule.h"

#include <string.h>

/* Return the number of digits of 'v' when converted to string in radix 10.
 * See ll2string() for more information. */
uint32_t GisModule_digits10(uint64_t v) {
    if (v < 10) return 1;
    if (v < 100) return 2;
    if (v < 1000) return 3;
    if (v < 1000000000000UL) {
        if (v < 100000000UL) {
            if (v < 1000000) {
                if (v < 10000) return 4;
                return (uint32_t) 5 + (v >= 100000);
            }
            return (uint32_t) 7 + (v >= 10000000UL);
        }
        if (v < 10000000000UL) {
            return (uint32_t) 9 + (v >= 1000000000UL);
        }
        return (uint32_t) 11 + (v >= 100000000000UL);
    }
    return 12 + GisModule_digits10(v / 1000000000000UL);
}

/* Convert a long long into a string. Returns the number of
 * characters needed to represent the number.
 * If the buffer is not big enough to store the string, 0 is returned.
 *
 * Based on the following article (that apparently does not provide a
 * novel approach but only publicizes an already used technique):
 *
 * https://www.facebook.com/notes/facebook-engineering/three-optimization-tips-for-c/10151361643253920
 *
 * Modified in order to handle signed integers since the original code was
 * designed for unsigned integers. */
int GisModule_ll2string(char *dst, size_t dstlen, long long svalue) {
    static const char digits[201] =
            "0001020304050607080910111213141516171819"
            "2021222324252627282930313233343536373839"
            "4041424344454647484950515253545556575859"
            "6061626364656667686970717273747576777879"
            "8081828384858687888990919293949596979899";
    int negative;
    unsigned long long value;

    /* The main loop works with 64bit unsigned integers for simplicity, so
     * we convert the number here and remember if it is negative. */
    if (svalue < 0) {
        if (svalue != LLONG_MIN) {
            value = -svalue;
        } else {
            value = ((unsigned long long) LLONG_MAX)+1;
        }
        negative = 1;
    } else {
        value = svalue;
        negative = 0;
    }

    /* Check length. */
    uint32_t const length = GisModule_digits10(value)+negative;
    if (length >= dstlen) return 0;

    /* Null term. */
    uint32_t next = length;
    dst[next] = '\0';
    next--;
    while (value >= 100) {
        int const i = (value % 100) * 2;
        value /= 100;
        dst[next] = digits[i + 1];
        dst[next - 1] = digits[i];
        next -= 2;
    }

    /* Handle last 1-2 digits. */
    if (value < 10) {
        dst[next] = '0' + (uint32_t) value;
    } else {
        int i = (uint32_t) value * 2;
        dst[next] = digits[i + 1];
        dst[next - 1] = digits[i];
    }

    /* Add sign. */
    if (negative) dst[0] = '-';
    return length;
}

int GisModule_DictInsertOrUpdate(RedisModuleDict *d, RedisModuleString *field, RedisModuleString *value) {
    int nokey = 0;
    RedisModuleString *oldvalue, *newvalue = RedisModule_CreateStringFromString(NULL, value);
    nokey = RedisModule_DictDel(d, field, &oldvalue);
    if (nokey == REDISMODULE_OK) {
        GisModule_FreeStringSafe(NULL, oldvalue);
    }
    return RedisModule_DictSet(d, field, newvalue);
}

void GisModule_FreeStringSafe(RedisModuleCtx *ctx, RedisModuleString *str) {
    if (!str) return;
    RedisModule_FreeString(ctx, str);
}

int GisModule_GetDoubleFromObjectOrReply(RedisModuleCtx *ctx, RedisModuleString *s, double *target, const char *msg) {
    double value;
    if (RedisModule_StringToDouble(s, &value) != REDISMODULE_OK) {
        if (msg != NULL) {
            RedisModule_ReplyWithError(ctx, (char *) msg);
        } else {
            RedisModule_ReplyWithError(ctx, "ERR value is not a valid float");
        }
        return REDISMODULE_ERR;
    }
    *target = value;
    return REDISMODULE_OK;
}

int GisModule_ExtractUnitOrReply(RedisModuleCtx *ctx, RedisModuleString *sunit, double *to_meters) {
    const char *unit = RedisModule_StringPtrLen(sunit, NULL);
    if (!strcasecmp(unit, "m")) {
        *to_meters = 1;
    } else if (!strcasecmp(unit, "km")) {
        *to_meters = 1000;
    } else if (!strcasecmp(unit, "ft")) {
        *to_meters = 0.3048;
    } else if (!strcasecmp(unit, "mi")) {
        *to_meters = 1609.34;
    } else {
        RedisModule_ReplyWithError(ctx,
                                   "ERR unsupported unit provided. please use m, km, ft, mi");
        return REDISMODULE_ERR;
    }
    return REDISMODULE_OK;
}

void GisModule_AddReplyDistance(RedisModuleCtx *ctx, double d) {
    char dbuf[128];
    size_t dlen = snprintf(dbuf, sizeof(dbuf), "%.4f", d);
    RedisModule_ReplyWithStringBuffer(ctx, dbuf, dlen);
}


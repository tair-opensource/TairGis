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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "zmalloc.h"
#include "geom.h"
#include "grisu3.h"
#include "geoutil.h"
#include "poly.h"
#include "json.h"

#ifndef LITTLE_ENDIAN
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define LITTLE_ENDIAN 1
#else
#define LITTLE_ENDIAN 0
#endif
#endif

static geomErr geomDecodeWKTInner(const char *input, geomWKTDecodeOpts opts, geom *g, int *size, int *read);

typedef struct ctx{
    geomErr err;
    geomWKTDecodeOpts opts;
    char *p;     // rolling pointer
    char *g;     // g buffer
    int len;     // length of g buffer

    char hasZ, hasM;  // is the current geometry defined 'Z' or 'M' or 'ZM'
    char isEmpty;     // is the current geometry defined 'EMPTY'
    char validBuffer; // is the g bytes buffer writable

    // placeholder for parsing doubles
    double x, y, z, m;
    int foundZ, foundM; // indicated that z and m in the previous read.

    int writtenCoord; // indicates that at least one coord has been written.
    int mustZ, mustM; // indicates that the stream must make space for Z or M.

} ctx;

const char *geomErrText(geomErr err){
    switch (err){
    default:
        return "unknown error";
    case GEOM_ERR_NONE:
        return "no error";
    case GEOM_ERR_INPUT:
        return "input";
    case GEOM_ERR_MEMORY:
        return "memory";
    case GEOM_ERR_UNSUPPORTED:
        return "unsupported";
    }
}

static ctx *ignorews(ctx *c){
    for (;;) {
        switch (c->p[0]) {
        default:
            return c;
        case '\t': case ' ': case '\r': case '\v': case '\n': case '\f':
            c->p++;
            continue;
        }
    }
}
/* grow will increase the c->g buffer by len. */
static ctx *grow(ctx *c, int len){
    if (!c->validBuffer){
        return c;
    }
    char *m = zrealloc(c->g, c->len+len);
    if (!m){
        c->err = GEOM_ERR_MEMORY;
        return c;
    }
    c->g = m;
    c->len += len;
    return c;
}

static ctx *appendByte(ctx *c, uint8_t b){
    if (!c->validBuffer){
        return c;
    }
    c = grow(c, 1);
    if (c->err){
        return c;
    }
    *(c->g+c->len-1) = b;
    return c;
}

static ctx *appendBytes(ctx *c, uint8_t *b, int sz){
    if (!c->validBuffer){
        return c;
    }
    c = grow(c, sz);
    if (c->err){
        return c;
    }
    memcpy(c->g+c->len-sz, b, sz);
    return c;
}

static ctx *appendUint32(ctx *c, uint32_t n){
    if (!c->validBuffer){
        return c;
    }
    c = grow(c, 4);
    if (c->err){
        return c;
    }
    *((uint32_t*)(c->g+c->len-4)) = n;
    return c;
}

static ctx *decodeHead(ctx *c){
    c->hasZ = 0;
    c->hasM = 0;
    c->isEmpty = 0;
    int read = 0;
read_next:
    switch (c->p[0]){
    case '\t': case ' ': case '\r': case '\v': case '\n': case '\f': 
    case 'z': case 'Z': case 'm': case 'M':
        c = ignorews(c);
        if ((c->p[0]=='E'||c->p[0]=='e')&&(c->p[1]=='M'||c->p[1]=='m')&&(c->p[2]=='P'||c->p[2]=='p')&&(c->p[3]=='T'||c->p[3]=='t')&&(c->p[4]=='Y'||c->p[4]=='y')){
            if (c->isEmpty){
                c->err = GEOM_ERR_INPUT;
                return c;    
            }
            c->p += 5;
            c->isEmpty = 1;
        } else if (c->p[0]=='Z'||c->p[0]=='z'){
            if (c->hasZ||c->hasM||c->isEmpty){
                c->err = GEOM_ERR_INPUT;
                return c;    
            }
            c->p++;
            c->hasZ = 1;
            if (c->p[0]=='M'||c->p[0]=='m'){
                c->p++;
                c->hasM = 1;
            }
        } else if (c->p[0]=='M'||c->p[0]=='m'){
            if (c->hasZ||c->hasM||c->isEmpty){
                c->err = GEOM_ERR_INPUT;
                return c;    
            }
            c->p++;
            c->hasM = 1;
        }
        break;
    }
    read++;
    if ((c->hasZ || c->hasM) && !c->isEmpty && read <= 2){
        goto read_next;
    }
    c = ignorews(c);
    if (!c->isEmpty){
        if (c->p[0]!='('){
            c->err = GEOM_ERR_INPUT;
            return c;
        }
    }
    return c;
}

static ctx *decodeNumbers(ctx *c){
    char *ptr = NULL;
    c->x = strtod(c->p, &ptr);
    if (ptr-c->p == 0){
        c->err = GEOM_ERR_INPUT;
        return c;
    }
    c->p += ptr-c->p;
    c = ignorews(c);
    c->y = strtod(c->p, &ptr);
    if (ptr-c->p == 0){
        c->err = GEOM_ERR_INPUT;
        return c;
    }
    c->p += ptr-c->p;
    if (c->hasZ){
        c = ignorews(c);
        c->z = strtod(c->p, &ptr);
        if (ptr-c->p == 0){
            c->err = GEOM_ERR_INPUT;
            return c;
        }
        c->p += ptr-c->p;
        c->foundZ = 1;
    } else {
        c->z = 0;
        if (c->hasM){
            c->foundZ = 0;
        } else {
            // Z is not specified, but we should see if it inputed.
            c = ignorews(c);
            if (c->p[0]==')'||c->p[0]==','){
                return c;
            }
            if (c->opts&GEOM_WKT_REQUIRE_ZM){
                c->err = GEOM_ERR_INPUT;
                return c;
            }
            c->z = strtod(c->p, &ptr);
            if (ptr-c->p == 0){
                c->err = GEOM_ERR_INPUT;
                return c;
            }
            c->p += ptr-c->p;
            c->foundZ = 1;
        }
    }
    if (c->hasM){
        c = ignorews(c);
        c->m = strtod(c->p, &ptr);
        if (ptr-c->p == 0){
            c->err = GEOM_ERR_INPUT;
            return c;
        }
        c->p += ptr-c->p;
        c->foundM = 1;
    } else {
        c->m = 0;
        if (c->hasZ){
           c->foundM = 0;
        } else{
            // M is not specified, but we should see if it inputed.
            c = ignorews(c);
            if (c->p[0]==')'||c->p[0]==','){
                return c;
            }
            if (c->opts&GEOM_WKT_REQUIRE_ZM){
                c->err = GEOM_ERR_INPUT;
                return c;
            }
            c->m = strtod(c->p, &ptr);
            if (ptr-c->p == 0){
                c->err = GEOM_ERR_INPUT;
                return c;
            }
            c->p += ptr-c->p;
            c->foundM = 1;
        }
    }
    return c;
}

static ctx *appendType(ctx *c, int type){
    if (c->hasZ && c->hasM){
        c = appendUint32(c, 3000+type);
    } else if (c->hasM){
        c = appendUint32(c, 2000+type);
    } else if (c->hasZ){
        c = appendUint32(c, 1000+type);
    } else {
        c = appendUint32(c, type);
    }
    return c;
}

static ctx *appendCoord(ctx *c){
    if (!c->validBuffer){
        return c;
    }
    int sz = 16;
    if (!c->writtenCoord){
        if (c->hasZ||c->foundZ){
            sz += 8;
            c->mustZ = 1;
        }
        if (c->hasM||c->foundM){
            sz += 8;
            c->mustM = 1;
        }
    } else {
        if (c->mustZ){
            sz += 8;
        }
        if (c->mustM){
            sz += 8;
        }
    }
    c = grow(c, sz);
    if (c->err){
        return c;
    }
    double *g = (double*)(c->g+c->len-sz);
    *(g++) = c->x;
    *(g++) = c->y;
    if (c->mustZ){
        *(g++) = c->z;
    }
    if (c->mustM){
        *(g++) = c->m;
    }
    if (!c->writtenCoord){
        // reset the type
        uint32_t type = *((uint32_t*)(c->g+1));
        if (type>3000) type -= 3000;
        else if (type>2000) type -= 2000;
        else if (type>1000) type -= 1000;
        if (c->mustZ && c->mustM){
            type = 3000+type;
        } else if (c->mustM){
            type = 2000+type;
        } else if (c->mustZ){
            type = 1000+type;
        }
        *((uint32_t*)(c->g+1)) = type;
    }
    c->writtenCoord = 1;

    return c;
}

static ctx *geomDecodePoint(ctx *c){
    c = decodeHead(c);
    if (c->err){
        return c;
    }
    c = appendType(c, GEOM_POINT);
    if (c->err){
        return c;
    }
    if (c->isEmpty){
        c->x = 0;
        c->y = 0;
        c->z = 0;
        c->m = 0;
    } else{
        if (c->p[0]!='('){
            c->err = GEOM_ERR_INPUT;
            return c;   
        }
        c->p++;
        c = ignorews(c);
        c = decodeNumbers(c);
        if (c->err){
            return c;
        }
        c = ignorews(c);
        if (c->p[0]!=')'){
            c->err = GEOM_ERR_INPUT;
            return c;   
        }
        c->p++;
    }
    c = appendCoord(c);

    return c;
}

static ctx *decodeSeriesSegment(ctx *c, int level){
    int szpos = c->len; // track the pos of size
    c = appendUint32(c, 0);
    if (c->err){
        return c;
    }
    if (c->isEmpty){
        return c;
    }
    if (c->p[0]!='('){
        c->err = GEOM_ERR_INPUT;
        return c;   
    }
    c->p++;
    c = ignorews(c);
    if (c->p[0]==')'){
        c->p++;
        return c;   
    }
    int sz = 0;
    for (;;){
        c = ignorews(c);
        if (level<0){
            // decode geometries
            geom g = NULL;
            int sz = 0;
            int read = 0;
            geomErr err = geomDecodeWKTInner(c->p, c->opts|GEOM_WKT_LEAVE_OPEN, &g, &sz, &read);
            if (err){
                c->err = err;
                return c;
            }
            c->p += read;
            c = appendBytes(c, (uint8_t*)g, sz);
            if (c->err){
                geomFree(g);
                return c;
            }
            geomFree(g);
        } else if (level==0){
            // decode the numbers
            c = decodeNumbers(c);
            if (c->err){
                return c;
            }
            c = appendCoord(c);
            if (c->err){
                return c;
            }
        } else {
            // decode nested series
            c = decodeSeriesSegment(c, level-1);
            if (c->err){
                return c;
            }
        }
        c = ignorews(c);
        sz++;
        if (c->p[0] == ','){
            c->p++;
            c = ignorews(c);
            continue;
        } else if (c->p[0] == ')'){
            c->p++;
            break;
        } else {
            c->err = GEOM_ERR_INPUT;
            return c;   
        }
    }
    *((uint32_t*)(c->g+szpos)) = sz;
    return c;
}

static ctx *geomDecodeSeries(ctx *c, geomType type){
    c = decodeHead(c);
    if (c->err){
        return c;
    }
    c = appendType(c, type);
    if (c->err){
        return c;
    }
    switch (type){
    default:
        c->err = GEOM_ERR_INPUT;
        return c;
    case GEOM_LINESTRING:
    case GEOM_MULTIPOINT:
        c = decodeSeriesSegment(c, 0);
        break;
    case GEOM_POLYGON:
    case GEOM_MULTILINESTRING:
        c = decodeSeriesSegment(c, 1);
        break;
    case GEOM_MULTIPOLYGON:
        c = decodeSeriesSegment(c, 2);
        break;
    case GEOM_GEOMETRYCOLLECTION:
        c = decodeSeriesSegment(c, -1);
        break;
    }
    return c;
}


ctx *geomDecodeGeometry(ctx *c){
    c = ignorews(c);
    if ((c->p[0]=='G'||c->p[0]=='g')&&(c->p[1]=='E'||c->p[1]=='e')&&(c->p[2]=='O'||c->p[2]=='o')&&(c->p[3]=='M'||c->p[3]=='m')&&(c->p[4]=='E'||c->p[4]=='e')&&(c->p[5]=='T'||c->p[5]=='t')&&(c->p[6]=='R'||c->p[6]=='r')&&(c->p[7]=='Y'||c->p[7]=='y')&&(c->p[8]=='C'||c->p[8]=='c')&&(c->p[9]=='O'||c->p[9]=='o')&&(c->p[10]=='L'||c->p[10]=='l')&&(c->p[11]=='L'||c->p[11]=='l')&&(c->p[12]=='E'||c->p[12]=='e')&&(c->p[13]=='C'||c->p[13]=='c')&&(c->p[14]=='T'||c->p[14]=='t')&&(c->p[15]=='I'||c->p[15]=='i')&&(c->p[16]=='O'||c->p[16]=='o')&&(c->p[17]=='N'||c->p[17]=='n')){
        c->p += 18;
        return geomDecodeSeries(c, GEOM_GEOMETRYCOLLECTION);
    } else if ((c->p[0]=='M'||c->p[0]=='m')&&(c->p[1]=='U'||c->p[1]=='u')&&(c->p[2]=='L'||c->p[2]=='l')&&(c->p[3]=='T'||c->p[3]=='t')&&(c->p[4]=='I'||c->p[4]=='i')){
        c->p += 5;
        if ((c->p[0]=='L'||c->p[0]=='l')&&(c->p[1]=='I'||c->p[1]=='i')&&(c->p[2]=='N'||c->p[2]=='n')&&(c->p[3]=='E'||c->p[3]=='e')&&(c->p[4]=='S'||c->p[4]=='s')&&(c->p[5]=='T'||c->p[5]=='t')&&(c->p[6]=='R'||c->p[6]=='r')&&(c->p[7]=='I'||c->p[7]=='i')&&(c->p[8]=='N'||c->p[8]=='n')&&(c->p[9]=='G'||c->p[9]=='g')){
            c->p += 10;
            return geomDecodeSeries(c, GEOM_MULTILINESTRING);
        } else if ((c->p[0]=='P'||c->p[0]=='p')&&(c->p[1]=='O'||c->p[1]=='o')){
            c->p += 2;
            if ((c->p[0]=='I'||c->p[0]=='i')&&(c->p[1]=='N'||c->p[1]=='n')&&(c->p[2]=='T'||c->p[2]=='t')){
                c->p += 3;
                return geomDecodeSeries(c, GEOM_MULTIPOINT);
            } else if ((c->p[0]=='L'||c->p[0]=='l')&&(c->p[1]=='Y'||c->p[1]=='y')&&(c->p[2]=='G'||c->p[2]=='g')&&(c->p[3]=='O'||c->p[3]=='o')&&(c->p[4]=='N'||c->p[4]=='n')){
                c->p += 5;
                return geomDecodeSeries(c, GEOM_MULTIPOLYGON);
            }       
        }
    } else if ((c->p[0]=='L'||c->p[0]=='l')&&(c->p[1]=='I'||c->p[1]=='i')&&(c->p[2]=='N'||c->p[2]=='n')&&(c->p[3]=='E'||c->p[3]=='e')&&(c->p[4]=='S'||c->p[4]=='s')&&(c->p[5]=='T'||c->p[5]=='t')&&(c->p[6]=='R'||c->p[6]=='r')&&(c->p[7]=='I'||c->p[7]=='i')&&(c->p[8]=='N'||c->p[8]=='n')&&(c->p[9]=='G'||c->p[9]=='g')){
        c->p += 10;
        return geomDecodeSeries(c, GEOM_LINESTRING);
    } else if ((c->p[0]=='P'||c->p[0]=='p')&&(c->p[1]=='O'||c->p[1]=='o')){
        c->p += 2;
        if ((c->p[0]=='I'||c->p[0]=='i')&&(c->p[1]=='N'||c->p[1]=='n')&&(c->p[2]=='T'||c->p[2]=='t')){
            c->p += 3;
            return geomDecodePoint(c);
        } else if ((c->p[0]=='L'||c->p[0]=='l')&&(c->p[1]=='Y'||c->p[1]=='y')&&(c->p[2]=='G'||c->p[2]=='g')&&(c->p[3]=='O'||c->p[3]=='o')&&(c->p[4]=='N'||c->p[4]=='n')){
            c->p += 5;
            return geomDecodeSeries(c, GEOM_POLYGON);
        }       
    }
    c->err = GEOM_ERR_INPUT;
    return c;
}

static geomErr geomDecodeWKTInner(const char *input, geomWKTDecodeOpts opts, geom *g, int *size, int *read){
    if (input == 0 || *input == 0){
        return GEOM_ERR_INPUT; 
    }
    ctx cc;
    memset(&cc, 0, sizeof(ctx));
    ctx *c = &cc;
    c->opts = opts;
    c->p = (char*)input;
    c->validBuffer = g && size;
    if (LITTLE_ENDIAN){
        c = appendByte(c, 0x01);
    } else{
        c = appendByte(c, 0x00);
    }
    if (c->err){
        return c->err;
    }
    c = ignorews(c);
    c = geomDecodeGeometry(c);
    if (c->err){
        zfree(c->g);
        return c->err;
    }
    if (!(opts&GEOM_WKT_LEAVE_OPEN)){
        c = ignorews(c);
        if (c->p[0]){
            // invalid characters found at end of input.
            zfree(c->g);
            return GEOM_ERR_INPUT; 
        }
    }
    if (g && size){
        *g = (geom)c->g;
        *size = c->len;
    }
    if (read){
        *read = c->p-input;
    }
    return GEOM_ERR_NONE;
}

static int geomIsZ(uint8_t *g){
    uint32_t type = *((uint32_t*)(g+1));
    if (type > 3000){
        return 1;
    } else if (type > 2000){
        return 0;
    } else if (type > 1000){
        return 1;
    }
    return 0;
}
static int geomIsM(uint8_t *g){
    uint32_t type = *((uint32_t*)(g+1));
    if (type > 3000){
        return 1;
    } else if (type > 2000){
        return 1;
    } else if (type > 1000){
        return 0;
    }
    return 0;
}

////////////////////////////////////////////////
// public apis
////////////////////////////////////////////////
geomType geomGetType(geom g){
    uint32_t type = *((uint32_t*)((uint8_t*)(g)+1));
    if (type > 3000){
        type = type-3000;
    } else if (type > 2000){
        type = type-2000;
    } else if (type > 1000){
        type = type-1000;
    }
    if (!GEOM_VALID_TYPE(type)){
        return GEOM_UNKNOWN;
    }
    return (geomType)type;
}

static inline char *appendStr(char *str, int *size, int *cap, char *s){
    int l = strlen(s);
    if (*size+l >= *cap){
        int ncap = *cap;
        if (ncap == 0){
            ncap = 16;
        }
        while (*size+l >= ncap){
            ncap *= 2;
        }
        char *nstr = zrealloc(str, ncap+1);
        if (!nstr){
            if (str){
                zfree(str);
            }
            return NULL;
        }
        str = nstr;
        *cap = ncap;
    }
    memcpy(str+(*size), s, l);
    (*size) += l;
    str[(*size)] = 0;
    return str;
}

static char *dstr(double n, char *str){
    dtoa_grisu3(n, str);
    return str;
}

void geomFreeWKT(char *wkt){
    if (wkt){
        zfree(wkt);
    }
}


static char *geomEncodeWKTInner(geom g, geomWKTEncodeOpts opts, int *read){
    #define APPEND(s) {if (!(str = appendStr(str, &size, &cap, (s)))) goto oom;}
    #define APPEND_COORD(){\
        APPEND(dstr(*((double*)gb), output));\
        gb += 8;\
        APPEND(" ");\
        APPEND(dstr(*((double*)gb), output));\
        gb += 8;\
        if (isZ){\
            APPEND(" ");\
            APPEND(dstr(*((double*)gb), output));\
            gb += 8;\
        }\
        if (isM){\
            APPEND(" ");\
            APPEND(dstr(*((double*)gb), output));\
            gb += 8;\
        }\
    }
    #define APPEND_HEAD(type){\
        if (showZM){\
            if (isZ&&isM){\
                APPEND(type " ZM")\
            }else if (isZ){\
                APPEND(type " Z")\
            }else if (isM){\
                APPEND(type " M")\
            }else{\
                APPEND(type)\
            }\
        } else{\
            APPEND(type)\
        }\
        gb+=5;\
    }

    #define APPEND_HEAD_SERIES(type){\
        APPEND_HEAD(type);\
        len = *((uint32_t*)(gb));\
        gb+=4;\
        if (len == 0){\
            if (opts&GEOM_WKT_SHOW_EMPTY){\
                APPEND(" EMPTY");\
            }else{\
                APPEND("()");\
            }\
            break;\
        }\
    }

    if (g == NULL){
        return NULL;
    }
    char *str = NULL;
    int size = 0;
    int cap = 0;
    char output[50];
    uint8_t *gb = (uint8_t*)g;
    int isZ = geomIsZ(gb);
    int isM = geomIsM(gb);
    int len = 0;
    int showZM = (isM&&!isZ)||(opts&GEOM_WKT_SHOW_ZM);
    char *wkt = NULL;

    int coordSize = 16;
    if (isZ){
        coordSize+=8;
    }
    if (isM){
        coordSize+=8;
    }
    geomType type = geomGetType(g);
    switch (type){
    default:
        return NULL;
    case GEOM_POINT:
        APPEND_HEAD("POINT");
        APPEND("(")
        APPEND_COORD();
        APPEND(")");
        break;
    case GEOM_LINESTRING:
    case GEOM_MULTIPOINT:
        if (type == GEOM_LINESTRING){
            APPEND_HEAD_SERIES("LINESTRING");
        } else {
            APPEND_HEAD_SERIES("MULTIPOINT");
        }
        APPEND("(")
        for (int i=0;i<len;i++){
            if (i!=0){
                APPEND(",");    
            }
            APPEND_COORD();
        }
        APPEND(")");
        break;
    case GEOM_POLYGON:
    case GEOM_MULTILINESTRING:
        if (type == GEOM_POLYGON){
            APPEND_HEAD_SERIES("POLYGON");
        } else {
            APPEND_HEAD_SERIES("MULTILINESTRING");
        }
        APPEND("(")
        for (int i=0;i<len;i++){
            if (i!=0){
                APPEND(",");
            }
            int len2 = *((uint32_t*)gb);
            gb += 4;
            if (len2 == 0){
                if (opts&GEOM_WKT_SHOW_EMPTY){
                    APPEND("EMPTY");
                } else{
                    APPEND("()");
                }
            } else {
                APPEND("(");
                for (int i=0;i<len2;i++){
                    if (i!=0){
                        APPEND(",");
                    }
                    APPEND_COORD();
                }
                APPEND(")");
            }
        }
        APPEND(")");
        break;
    case GEOM_MULTIPOLYGON:
        APPEND_HEAD_SERIES("MULTIPOLYGON");
        APPEND("(")
        for (int i=0;i<len;i++){
            if (i!=0){
                APPEND(",");
            }
            int len2 = *((uint32_t*)gb);
            gb += 4;
            if (len2 == 0){
                if (opts&GEOM_WKT_SHOW_EMPTY){
                    APPEND("EMPTY");
                } else{
                    APPEND("()");
                }
            } else {
                APPEND("(");
                for (int i=0;i<len2;i++){
                    if (i!=0){
                        APPEND(",");
                    }
                    int len3 = *((uint32_t*)gb);
                    gb += 4;
                    if (len3 == 0){
                        if (opts&GEOM_WKT_SHOW_EMPTY){
                            APPEND("EMPTY");
                        } else{
                            APPEND("()");
                        }
                    } else {
                        APPEND("(");
                        for (int i=0;i<len3;i++){
                            if (i!=0){
                                APPEND(",");
                            }
                            APPEND_COORD();
                        }
                        APPEND(")");
                    }
                }
                APPEND(")");
            }
        }
        APPEND(")");
        break;
    case GEOM_GEOMETRYCOLLECTION:
        APPEND_HEAD_SERIES("GEOMETRYCOLLECTION");
        APPEND("(");
        for (int i=0;i<len;i++){
            if (i!=0){
                APPEND(",");
            }
            int read = 0;
            wkt = geomEncodeWKTInner((geom)gb, opts, &read);
            if (!wkt){
                goto oom;
            }
            gb += read;
            APPEND(wkt);
            geomFreeWKT(wkt);
            wkt = NULL;
        }
        APPEND(")");
        break;
    }
    if (read){
        *read = (void*)gb-(void*)g;
    }
    if (wkt){
        geomFreeWKT(wkt);
    }
    return str;
oom:
    if (str){
        zfree(str);
    }
    if (wkt){
        geomFreeWKT(wkt);
    }
    return NULL;
}

void geomFree(geom g){
    if (g){
        zfree(g);
    }
}

char *geomEncodeWKT(geom g, geomWKTEncodeOpts opts){
    return geomEncodeWKTInner(g, opts, NULL);
}

static char *geomEncodeJSONInner(geom g, int *read){
    #define JSON_APPEND(s) {if (!(str = appendStr(str, &size, &cap, (s)))) goto oom;}
    #define JSON_APPEND_COORD(){\
        JSON_APPEND(dstr(*((double*)gb), output));\
        gb += 8;\
        JSON_APPEND(",");\
        JSON_APPEND(dstr(*((double*)gb), output));\
        gb += 8;\
        if (isZ){\
            JSON_APPEND(",");\
            JSON_APPEND(dstr(*((double*)gb), output));\
            gb += 8;\
        }\
        if (isM){\
            if (!isZ){\
                JSON_APPEND(",0");\
            }\
            JSON_APPEND(",");\
            JSON_APPEND(dstr(*((double*)gb), output));\
            gb += 8;\
        }\
    }
    #define JSON_APPEND_HEAD(type){\
        APPEND("{\"type\":\"");\
        APPEND(type);\
        APPEND("\",\"coordinates\":");\
        gb+=5;\
    }
    #define JSON_APPEND_HEAD_COLL(type){\
        APPEND("{\"type\":\"");\
        APPEND(type);\
        APPEND("\",\"geometries\":");\
        gb+=5;\
    }

    #define JSON_APPEND_HEAD_SERIES(type){\
        JSON_APPEND_HEAD(type);\
        len = *((uint32_t*)(gb));\
        gb+=4;\
        if (len == 0){\
            APPEND("[]}");\
            break;\
        }\
    }
    #define JSON_APPEND_HEAD_SERIES_COLL(type){\
        JSON_APPEND_HEAD_COLL(type);\
        len = *((uint32_t*)(gb));\
        gb+=4;\
        if (len == 0){\
            APPEND("[]}");\
            break;\
        }\
    }



    if (g == NULL){
        return NULL;
    }
    char *str = NULL;
    int size = 0;
    int cap = 0;
    char output[50];
    uint8_t *gb = (uint8_t*)g;
    int isZ = geomIsZ(gb);
    int isM = geomIsM(gb);
    int len = 0;
    char *json = NULL;

    int coordSize = 16;
    if (isZ){
        coordSize+=8;
    }
    if (isM){
        coordSize+=8;
    }
    geomType type = geomGetType(g);
    switch (type){
    default:
        return NULL;
    case GEOM_POINT:
        JSON_APPEND_HEAD("Point");
        JSON_APPEND("[")
        JSON_APPEND_COORD();
        JSON_APPEND("]}");
        break;
    case GEOM_LINESTRING:
    case GEOM_MULTIPOINT:
        if (type == GEOM_LINESTRING){
            JSON_APPEND_HEAD_SERIES("LineString");
        } else {
            JSON_APPEND_HEAD_SERIES("MultiPoint");
        }
        JSON_APPEND("[")
        for (int i=0;i<len;i++){
            if (i!=0){
                JSON_APPEND(",");    
            }
            JSON_APPEND("[")
            JSON_APPEND_COORD();
            JSON_APPEND("]");
        }
        JSON_APPEND("]}");
        break;
    case GEOM_POLYGON:
    case GEOM_MULTILINESTRING:
        if (type == GEOM_POLYGON){
            JSON_APPEND_HEAD_SERIES("Polygon");
        } else {
            JSON_APPEND_HEAD_SERIES("MultiLineString");
        }
        JSON_APPEND("[")
        for (int i=0;i<len;i++){
            if (i!=0){
                JSON_APPEND(",");
            }
            int len2 = *((uint32_t*)gb);
            gb += 4;
            JSON_APPEND("[");
            for (int i=0;i<len2;i++){
                if (i!=0){
                    JSON_APPEND(",");
                }
                JSON_APPEND("[");
                JSON_APPEND_COORD();
                JSON_APPEND("]");
            }
            JSON_APPEND("]");
        }
        JSON_APPEND("]}");
        break;
    case GEOM_MULTIPOLYGON:
        JSON_APPEND_HEAD_SERIES("MultiPolygon");
        JSON_APPEND("[")
        for (int i=0;i<len;i++){
            if (i!=0){
                JSON_APPEND(",");
            }
            int len2 = *((uint32_t*)gb);
            gb += 4;
            JSON_APPEND("[");
            for (int i=0;i<len2;i++){
                if (i!=0){
                    JSON_APPEND(",");
                }
                int len3 = *((uint32_t*)gb);
                gb += 4;
                JSON_APPEND("[");
                for (int i=0;i<len3;i++){
                    if (i!=0){
                        JSON_APPEND(",");
                    }
                    JSON_APPEND("[");
                    JSON_APPEND_COORD();
                    JSON_APPEND("]");
                }
                JSON_APPEND("]");
            }
            JSON_APPEND("]");
        }
        JSON_APPEND("]}");
        break;
    case GEOM_GEOMETRYCOLLECTION:
        JSON_APPEND_HEAD_SERIES_COLL("GeometryCollection");
        JSON_APPEND("[");
        for (int i=0;i<len;i++){
            if (i!=0){
                JSON_APPEND(",");
            }
            int read = 0;
            json = geomEncodeJSONInner((geom)gb, &read);
            if (!json){
                goto oom;
            }
            gb += read;
            JSON_APPEND(json);
            geomFreeJSON(json);
            json = NULL;
        }
        APPEND("]}");
        break;
    }
    if (read){
        *read = (void*)gb-(void*)g;
    }
    if (json){
        geomFreeJSON(json);
    }
    return str;
oom:
    if (str){
        zfree(str);
    }
    if (json){
        geomFreeJSON(json);
    }
    return NULL;
}




char *geomEncodeJSON(geom g){
    return geomEncodeJSONInner(g, NULL);
}

void geomFreeJSON(char *json){
    if (json){
        zfree(json);
    }
}



geomErr geomDecodeWKT(const char *input, geomWKTDecodeOpts opts, geom *g, int *size){
    return geomDecodeWKTInner(input, opts, g, size, NULL);
}

typedef struct ghdr {
    int z, m;
    int type;
} ghdr;

static inline ghdr readhdr(uint8_t *g){
    ghdr h;
    uint32_t type = ((uint32_t*)g)[0];
    if (type >= 3000 && type <= 3999){
        h.z = 1;
        h.m = 1;
        h.type = type - 3000;
    } else if (type >= 2000 && type <= 2999){
        h.z = 0;
        h.m = 1;
        h.type = type - 2000;
    } else if (type >= 1000 && type <= 1999){
        h.z = 1;
        h.m = 0;
        h.type = type - 1000;
    } else if (type <= 999){
        h.z = 0;
        h.m = 0;
        h.type = type;
    } else {
        h.z = 0;
        h.m = 0;
        h.type = GEOM_UNKNOWN;
    }
    if (!GEOM_VALID_TYPE(h.type)){
        h.type = GEOM_UNKNOWN;
    }
    return h;
}

static inline geomCoord readcoord(ghdr *h, uint8_t *g, int *sz){
    geomCoord coord;
    coord.x = ((double*)g)[0];
    coord.y = ((double*)g)[1];
    if (h->z){
        coord.z = ((double*)g)[2];
        if (h->m){
            coord.m = ((double*)g)[3];
            if (sz){
                *sz = 4*8;
            }
        } else{
            coord.m = 0;
            if (sz){
                *sz = 3*8;
            }
        }
    } else if (h->m) {
        coord.z = 0;
        coord.m = ((double*)g)[2];
        if (sz){
            *sz = 3*8;
        }
    } else {
        coord.z = 0;
        coord.m = 0;
        if (sz){
           *sz = 2*8;
        }
    }
    return coord;
}

geomCoord geomRectCenter(geomRect r){
    geomCoord p;
    p.x = (r.max.x-r.min.x)/2+r.min.x;
    p.y = (r.max.y-r.min.y)/2+r.min.y;
    p.z = (r.max.z-r.min.z)/2+r.min.z;
    p.m = (r.max.m-r.min.m)/2+r.min.m;
    return p;
}

geomRect geomRectExpand(geomRect r, geomCoord p){
    if (p.x < r.min.x) r.min.x = p.x;
    if (p.y < r.min.y) r.min.y = p.y;
    if (p.z < r.min.z) r.min.z = p.z;
    if (p.m < r.min.m) r.min.m = p.m;
    if (p.x > r.max.x) r.max.x = p.x;
    if (p.y > r.max.y) r.max.y = p.y;
    if (p.z > r.max.z) r.max.z = p.z;
    if (p.m > r.max.m) r.max.m = p.m;
    return r;
}

geomRect geomRectUnion(geomRect r1, geomRect r2){
    r1 = geomRectExpand(r1, r2.min);
    r1 = geomRectExpand(r1, r2.max);
    return r1;
}

int geomCoordString(geomCoord c, int withZ, int withM, char *str){
    char *p = str;
    p += dtoa_grisu3(c.x, p);
    *(p++) = ' ';
    p += dtoa_grisu3(c.y, p);
    if (withZ){
        *(p++) = ' ';
        p += dtoa_grisu3(c.z, p);
        if (withM){
            *(p++) = ' ';
            p += dtoa_grisu3(c.m, p);            
        }
    } else if (withM){
        *(p++) = ' ';
        p += dtoa_grisu3(c.m, p);            
    }
    *(p++) = '\0';
    return p-str-1;
}


int geomRectString(geomRect r, int withZ, int withM, char *str){
    char *p = str;
    p += geomCoordString(r.min, withZ, withM, p);
    *(p++) = ',';
    p += geomCoordString(r.max, withZ, withM, p);
    *(p++) = '\0';
    return str-p-1;
}


#define FROM_GEOM_C
#include "geom_levels.c"
#include "geom_polymap.c"
#include "geom_json.c"


// geomGetCoord return any coord that is contained somewhere within the specified geometry.
geomCoord geomCenter(geom g){
    return levelAny_geomCenter((uint8_t*)g, NULL);
}

geomRect geomBounds(geom g){
    return levelAny_geomBounds((uint8_t*)g, NULL);
}

geomErr geomDecodeWKB(const void *input, size_t length, geom *g, int *size){
    geomErr err;
    char *p = (char*)input;
    if (length==0){
        err = GEOM_ERR_INPUT;
        goto err;
    }
    // TODO: check and translate wkb BE/LE to host and verify that the binary is correct.
    // Until that happens, crashes could occur when loading data from a incompatible 
    // byte order CPU, and buffer overflows for invalid inputs.
    switch (p[0]){
    default:
        err = GEOM_ERR_INPUT;
        goto err;
    case 0:
        if (LITTLE_ENDIAN){
            err = GEOM_ERR_INPUT;
            goto err;
        }
    case 1:
        if (!LITTLE_ENDIAN){
            err = GEOM_ERR_INPUT;
            goto err;
        }
    }
    if (g && size){
        *g = zmalloc(length);
        if (!*g){
            err = GEOM_ERR_MEMORY;
            goto err;
        }
        memcpy(*g, input, length);
        *size = length;
    }
    return GEOM_ERR_NONE;
err:
    return err;
}




geomErr geomDecodeJSON(const void *input, size_t length, geom *g, int *size){
    geomErr err;
    json_value *value = json_parse(input, length);
    if (!value){
        err = GEOM_ERR_INPUT;
        goto err;
    }
    int res = jsonDecodeAny(value, g, size);
    if (!res){
        err = GEOM_ERR_INPUT;
        goto err;
    }
    json_value_free(value);
    return GEOM_ERR_NONE;
err:
    if (value){
        json_value_free(value);
    }
    return err;
}

geomErr geomDecode(const void *input, size_t length, geomWKTDecodeOpts opts, geom *g, int *size){
    char *bytes = (char*)input;
    if (length > 0){
        switch (bytes[0]){
        default:
            return geomDecodeWKT(input, opts, g, size);
        case 0: case 1:
            return geomDecodeWKB(input, length, g, size);
        case '{':
            return geomDecodeJSON(input, length, g, size);
        case '\t': case ' ': case '\r': case '\v': case '\n': case '\f':
            for (int i=0;i<length;i++){
                switch (bytes[i]){
                default:
                    return geomDecodeWKT(input, opts, g, size);
                case '\t': case ' ': case '\r': case '\v': case '\n': case '\f':
                    continue;
                case '{':
                    // future geojson support
                    return GEOM_ERR_UNSUPPORTED;
                }
            }
            break;
        }
    }
    return GEOM_ERR_INPUT;
}

// CirclePolygon returns a Polygon around the radius.
geom geomNewCirclePolygon(geomCoord center, double meters, int steps, int *size){
    if (steps < 3) {
        steps = 3;
    }
    int sz = ((steps+1)*16)+13; // exact byte count
    uint8_t *b = zmalloc(sz);
    if (!b){
        return NULL;
    }
    if (LITTLE_ENDIAN){
        b[0] = 0x01;
    } else{
        b[0] = 0x00;
    }
    *((uint32_t*)(b+1)) = 3;
    *((uint32_t*)(b+5)) = 1;
    *((uint32_t*)(b+9)) = steps+1;
    geomCoord p = {0,0,0,0};
    double step = 360.0 / (double)steps;
    int i = 0;
    for (double deg=0;deg<360.0;deg+=step) {
        geoutilDestinationLatLon(center.y, center.x, meters, deg, &p.y, &p.x);
        *((double*)(b+13+(i*16)+0)) = p.x;
        *((double*)(b+13+(i*16)+8)) = p.y;
        i++;
    }
    *((double*)(b+13+(i*16)+0)) = *((double*)(b+13+(0*16)+0));
    *((double*)(b+13+(i*16)+8)) = *((double*)(b+13+(0*16)+8));
    i++;
    if (size) *size = sz;
    return (geom)b;
}

geom geomNewRectPolygon(geomRect rect, int *size){
    int sz = (5*16)+13; // exact byte count
    uint8_t *b = zmalloc(sz);
    if (!b){
        return NULL;
    }
    if (LITTLE_ENDIAN){
        b[0] = 0x01;
    } else{
        b[0] = 0x00;
    }
    *((uint32_t*)(b+1)) = 3;
    *((uint32_t*)(b+5)) = 1;
    *((uint32_t*)(b+9)) = 5;
    double *values = (double*)(b+13);
    values[0] = rect.min.x;
    values[1] = rect.max.y;
    values[2] = rect.max.x;
    values[3] = rect.max.y;
    values[4] = rect.max.x;
    values[5] = rect.min.y;
    values[6] = rect.min.x;
    values[7] = rect.min.y;
    values[8] = rect.min.x;
    values[9] = rect.max.y;
    if (size) *size = sz;
    return (geom)b;
}


int geomIsSimplePoint(geom g){
    if (g){
        switch (*((uint32_t*)(((uint8_t*)g)+1))){
        case 1: case 1001: case 2001: case 3001:
            return 1;
        }
    }
    return 0;
}

geomErr geomGeometryCollectionIterator(geom g, geomIterator *itr){
    if (!g || !itr){
        return GEOM_ERR_INPUT;
    }
    uint8_t *ptr = (uint8_t*)g;
    ptr++;
    ghdr h = readhdr(ptr);
    if (h.type!=GEOM_GEOMETRYCOLLECTION){
        return GEOM_ERR_INPUT;
    }
    ptr+=4;
    int count = *((uint32_t*)ptr);
    ptr+=4;
    memset(itr, 0, sizeof(geomIterator));
    itr->count = count; 
    itr->ptr = ptr;
    return GEOM_ERR_NONE;
}

int geomIteratorValues(geomIterator *itr, geom *g, int *sz){
    if (!itr){
        return 0;
    }
    if (g){
        *g = itr->g;
    }
    if (sz){
        *sz = itr->sz;
    }
    return 1;
}

int geomIteratorNext(geomIterator *itr){
    if (!itr){
        return 0;
    }
    if (!itr->count){
        return 0;
    }
    itr->g = 0;
    itr->sz = 0;
    uint8_t *optr = itr->ptr;
    itr->ptr++;
    ghdr h = readhdr(itr->ptr);
    itr->ptr+=4;
    if (h.type==GEOM_UNKNOWN){
        return 0;
    }
    int dims = 2;
    if (h.z){
        dims++;
    }
    if (h.m){
        dims++;
    }
    switch(h.type){
    case GEOM_GEOMETRYCOLLECTION:{
        // nested geometry collection. but hey why not...
        // we have to iterate through it in order to get to the end of it.
        // sigh...
        geomIterator itr2;
        geomErr err = geomGeometryCollectionIterator((geom)optr, &itr2);
        if (err != GEOM_ERR_NONE){
            return 0;
        }
        while (geomIteratorNext(&itr2));
        itr->ptr = itr2.ptr;
        break;
    }
    case GEOM_POINT:{
        itr->ptr += (dims*8);
        break;
    }
    case GEOM_MULTIPOINT:
    case GEOM_LINESTRING:{
        int count = *((uint32_t*)itr->ptr);
        itr->ptr+=4;
        itr->ptr+=count*(dims*8);
        break;
    }
    case GEOM_MULTILINESTRING:
    case GEOM_POLYGON:{
        int count = *((uint32_t*)itr->ptr);
        itr->ptr+=4;
        for (int i=0;i<count;i++){
            int count2 = *((uint32_t*)itr->ptr);
            itr->ptr+=4;
            itr->ptr+=count2*(dims*8);
        }
        break;
    }
    case GEOM_MULTIPOLYGON:{
        int count = *((uint32_t*)itr->ptr);
        itr->ptr+=4;
        for (int i=0;i<count;i++){
            int count2 = *((uint32_t*)itr->ptr);
            itr->ptr+=4;
            for (int j=0;j<count2;j++){
                int count3 = *((uint32_t*)itr->ptr);
                itr->ptr+=4;
                itr->ptr+=count3*(dims*8);
            }
        }
        break;
    }}
    itr->g = (geom)optr;
    itr->sz = itr->ptr-optr;
    itr->count--;
    return 1;
}

geom *geomGeometryCollectionFlattenedArray(geom g, int *count){
    geom *gg = NULL;
    geomIterator itr;
    geomErr err = geomGeometryCollectionIterator(g, &itr);
    if (err != GEOM_ERR_NONE){
        goto err;
    }
    gg = zmalloc(0);
    if (!gg){
        goto err;
    }
    int len = 0;
    int cap = 0;
    geom ig = NULL;
    while (geomIteratorNext(&itr)){
        if (!geomIteratorValues(&itr, &ig, NULL)){
            goto err;
        }
        int release = 0;
        geom *arr = &ig;
        int arrn = 1;
        if (geomGetType(ig) == GEOM_GEOMETRYCOLLECTION){
            release = 1;
            arr = geomGeometryCollectionFlattenedArray(ig, &arrn);
            if (!arr){
                goto err;
            }
        }
        for (int i=0;i<arrn;i++){
            geom ig2 = arr[i];
            if (len==cap){
                int ncap = cap==0?1:cap*2;
                geom *ngg = zrealloc(gg, ncap*sizeof(geom));
                if (!ngg){
                    if (release){
                        geomFreeFlattenedArray(arr);
                    }
                    goto err;
                } 
                gg = ngg;
                cap = ncap;
            }
            gg[len++] = ig2;
        }
        if (release){
            geomFreeFlattenedArray(arr);
        }
    }
    if (count){
        *count = len;
    }
    return gg;
err:
    zfree(gg);
    return NULL;
}
void geomFreeFlattenedArray(geom *garr){
    if (garr){
        zfree(garr);
    }
}
int geomCoordWithinRadius(geomCoord c, geomCoord center, double meters){
    return geoutilDistance(c.y, c.x, center.y, center.x) <= meters ? 1 : 0;
}

static int pointContains(polyPoint a, geomPolyMap *m){
    for (int i=0;i<m->polygonCount;i++){
        switch (m->types[i]){
        default:
            return 0;
        case GEOM_POINT:{
            polyPoint b = polyPolygonPoint(m->polygons[i],0);
            if (a.x == b.x && a.y == b.y){
                return 1;
            }
            break;
        }
        }
    }
    return 0;
}

static int pointWithin(polyPoint a, geomPolyMap *m){
    for (int i=0;i<m->polygonCount;i++){
        switch (m->types[i]){
        default:
            return 0;
        case GEOM_POINT:{
            polyPoint b = polyPolygonPoint(m->polygons[i],0);
            if (a.x == b.x && a.y == b.y){
                return 1;
            }
            break;
        }
        case GEOM_LINESTRING:
            for (int j=1;j<m->polygons[i].len;j++){
                polyPoint b = polyPolygonPoint(m->polygons[i],j);
                polyPoint c = polyPolygonPoint(m->polygons[i],(j+1)%m->polygons[i].len);
                if (polyRaycast(a, b, c)==RAY_ON){
                    return 1;
                }
            }
            break;
        case GEOM_POLYGON:
            if (polyPointInside(a, m->polygons[i], m->holes[i])){
                return 1;
            }
            break;
        }
    }
    return 0;
}

static int lineIntersects(polyPoint a, polyPoint b, geomPolyMap *m){
    for (int i=0;i<m->polygonCount;i++){
        switch (m->types[i]){
        default:
            return 0;
        case GEOM_POINT:{
            polyPoint c = polyPolygonPoint(m->polygons[i],0);
            if (polyRaycast(c, a, b)==RAY_ON){
                return 1;
            }
            break;
        }
        case GEOM_LINESTRING:
            for (int j=1;j<m->polygons[i].len;j++){
                polyPoint c = polyPolygonPoint(m->polygons[i],j);
                polyPoint d = polyPolygonPoint(m->polygons[i],j);
                if (polyLinesIntersect(a,b,c,d)){
                    return 1;
                }
            }
            break;
		/*
        case GEOM_POLYGON:
            if (polyPointInside(a, m->polygons[i], m->holes[i])||
                polyPointInside(b, m->polygons[i], m->holes[i])){
                return 1;
            }
            break;*/
        }
    }
    return 0;
}

static int lineContains(polyPoint a, polyPoint b, geomPolyMap *m){
    for (int i=0;i<m->polygonCount;i++){
        switch (m->types[i]){
        default:
            return 0;
        case GEOM_POINT:{
            polyPoint c = polyPolygonPoint(m->polygons[i],0);
            if (polyRaycast(c, a, b)==RAY_ON){
                return 1;
            }
            break;
        }
        case GEOM_LINESTRING:
            for (int j=1;j<m->polygons[i].len;j++){
                polyPoint c = polyPolygonPoint(m->polygons[i],j);
                polyPoint d = polyPolygonPoint(m->polygons[i],(j+1)%m->polygons[i].len);
	        polyPoint mid = lineCenter(a,b);
                if (polyRaycast(c,a,b)==RAY_ON&&
					(polyRaycast(mid,a,b)==RAY_ON&&
					(polyRaycast(d,a,b))==RAY_ON)){
                    return 1;
                }
            }
            break;
       /* case GEOM_POLYGON:
            if (polyPointInside(a, m->polygons[i], m->holes[i])||
                polyPointInside(b, m->polygons[i], m->holes[i])){
                return 1;
            }
            break;*/
        }
    }
    return 0;
}

static int lineExIntersects(polyPoint a, polyPoint b, geomPolyMap *m){
    for (int i=0;i<m->polygonCount;i++){
        switch (m->types[i]){
        default:
            return 0;
        case GEOM_POINT:{
            polyPoint c = polyPolygonPoint(m->polygons[i],0);
            if (polyRaycast(c, a, b)==RAY_ON){
                return 1;
            }
            break;
        }
        case GEOM_LINESTRING:
            for (int j=1;j<m->polygons[i].len;j++){
                polyPoint c = polyPolygonPoint(m->polygons[i],j);
                polyPoint d = polyPolygonPoint(m->polygons[i],(j+1)%m->polygons[i].len);
		polyPoint mid = lineCenter(c,d);
                if (polyLinesIntersect(a,b,c,d)&&
					(!((polyRaycast(c,a,b)==RAY_ON)&&
					(polyRaycast(d,a,b)==RAY_ON)&&
					(polyRaycast(a,b,mid)==RAY_ON)))){
                    return 1;
                }
            }
            break;
        case GEOM_POLYGON:
            if (polyPointInside(a, m->polygons[i], m->holes[i])||
                polyPointInside(b, m->polygons[i], m->holes[i])){
                return 1;
            }
            break;
        }
    }
    return 0;
}

static int polygonContains(polyPolygon polygon, polyMultiPolygon holes, geomPolyMap *m){
    for (int i=0;i<m->polygonCount;i++){
        switch (m->types[i]){
        default:
            return 0;
        case GEOM_POINT:{
            polyPoint a = polyPolygonPoint(m->polygons[i],0);
            if (polyPointInside(a, polygon, holes)){
                return 1;
            }
            break;
        }
        case GEOM_LINESTRING:
            for (int j=1;j<m->polygons[i].len;j++){
                polyPoint a = polyPolygonPoint(m->polygons[i],j);
                polyPoint b = polyPolygonPoint(m->polygons[i],(j+1)%m->polygons[i].len);
                if (polyLineInside(a, b, polygon, holes)){
                    return 1;
                }            
            }
            break; 
        case GEOM_POLYGON:
            if (polyPolygonContains(polygon, m->polygons[i], m->holes[i])){
                return 1;
            }
            break;
        }
    }
    return 0;
}

static int polygonIntersects(polyPolygon polygon, polyMultiPolygon holes, geomPolyMap *m){
    for (int i=0;i<m->polygonCount;i++){
        switch (m->types[i]){
        default:
            return 0;
        case GEOM_POINT:{
            polyPoint a = polyPolygonPoint(m->polygons[i],0);
            if (polyPointInside(a, polygon, holes)){
                return 1;
            }
            break;
        }
        case GEOM_LINESTRING:
            for (int j=1;j<m->polygons[i].len;j++){
                polyPoint a = polyPolygonPoint(m->polygons[i],j);
                polyPoint b = polyPolygonPoint(m->polygons[i],j);
                if (polyPointInside(a, polygon, holes)||
                    polyPointInside(b, polygon, holes)){
                    return 1;
                }            
            }
            break;
        case GEOM_POLYGON:
            if (polyPolygonIntersects(polygon, m->polygons[i], m->holes[i])){
                return 1;
            }
            break;
        }
    }
    return 0;
}

static int polygonExIntersects(polyPolygon polygon, polyMultiPolygon holes, geomPolyMap *m){
    for (int i=0;i<m->polygonCount;i++){
        switch (m->types[i]){
        default:
            return 0;
        case GEOM_POINT:{
            polyPoint a = polyPolygonPoint(m->polygons[i],0);
            if (polyPointInside(a, polygon, holes)){
                return 1;
            }
            break;
        }
        case GEOM_LINESTRING:
            for (int j=1;j<m->polygons[i].len;j++){
                polyPoint a = polyPolygonPoint(m->polygons[i],j);
                polyPoint b = polyPolygonPoint(m->polygons[i],(j+1)%m->polygons[i].len);
                if (polyLineIntersect(a,b,polygon,holes)){
                    return 1;
                } 
            }
            break;
        case GEOM_POLYGON:
            if (polyPolygonIntersects(polygon, m->polygons[i], m->holes[i])){
                return 1;
            }
            break;
        }
    }
    return 0;
}

int geomPolyMapIntersects(geomPolyMap *m1, geomPolyMap *m2){
    if (!m1 || !m2){
        return 0;
    }
    for (int i=0;i<m1->polygonCount;i++){
        switch (m1->types[i]){
        default:
            return 0;
        case GEOM_POINT:
            if (m1->polygons[i].len && 
                pointWithin(polyPolygonPoint(m1->polygons[i], 0), m2)){
                return 1;
            }
            break;
        case GEOM_LINESTRING:
            for (int j=1;j<m1->polygons[i].len;j++){
                polyPoint a = polyPolygonPoint(m1->polygons[i],j);
                polyPoint b = polyPolygonPoint(m1->polygons[i],j);
                if (lineIntersects(a, b, m2)){
                    return 1;
                }
            }
            break;
        case GEOM_POLYGON:
            if (polygonIntersects(m1->polygons[i], m1->holes[i], m2)){
                return 1;
            }
            break;
        }
    }
    return 0;
}

int geomPolyMapWithin(geomPolyMap *m1, geomPolyMap *m2){
    if (!m1 || !m2){
        return 0;
    }
    if (m1->polygonCount==0){
        return 0;
    }
    for (int i=0;i<m1->polygonCount;i++){
        for (int j=0;j<m1->polygons[i].len;j++){
            if (!pointWithin(polyPolygonPoint(m1->polygons[i],j),m2)){
                return 0;
            }
        }
    }
    return 1;
}

int geomPolyMapContains(geomPolyMap *m1, geomPolyMap *m2){
    if (!m1 || !m2){
        return 0;
    }

	for (int i=0;i<m1->polygonCount;i++){
		switch (m1->types[i]){
		default:
            return 0;
		case GEOM_POINT:
            if (m1->polygons[i].len && 
                pointContains(polyPolygonPoint(m1->polygons[i], 0), m2)){
                return 1;
            }
            break;
		case GEOM_LINESTRING:
            for (int j=1;j<m1->polygons[i].len;j++){
                polyPoint a = polyPolygonPoint(m1->polygons[i],j);
                polyPoint b = polyPolygonPoint(m1->polygons[i],(j+1)%m1->polygons[i].len);
                if (lineContains(a, b, m2)){
                    return 1;
                }
            }
            break;
		case GEOM_POLYGON:
            if (polygonContains(m1->polygons[i], m1->holes[i], m2)){
                return 1;
            }
            break;
		}
	}
	return 0;
}

int geomPolyMapExIntersects(geomPolyMap *m1, geomPolyMap *m2){
    if (!m1 || !m2){
        return 0;
    }

    for (int i=0;i<m1->polygonCount;i++){
        switch (m1->types[i]){
	    default:
                return 0;
	    case GEOM_POINT:
	        if (m1->polygons[i].len && 
                   pointWithin(polyPolygonPoint(m1->polygons[i], 0), m2)){
                   return 1;
                }
                break;
            case GEOM_LINESTRING:
                for (int j=1;j<m1->polygons[i].len;j++){
                   polyPoint a = polyPolygonPoint(m1->polygons[i],j);
                   polyPoint b = polyPolygonPoint(m1->polygons[i],(j+1)%m1->polygons[i].len);
                   if(lineExIntersects(a, b, m2)){
                       return 1;
		   }
		}
                break;
            case GEOM_POLYGON:
                if (polygonExIntersects(m1->polygons[i], m1->holes[i], m2)){
                    return 1;
                }
                break;
	   }
				
	}
    return 0;
}

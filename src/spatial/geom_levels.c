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

#ifndef FROM_GEOM_C
#error This is not a standalone source file.
#endif

static geomRect levelAny_geomBounds(uint8_t *g, int *read);
static geomCoord levelAny_geomCenter(uint8_t *g, int *read);


static geomRect level1_getBounds(ghdr *h, uint8_t *g, int *read){
	geomRect r;
    r.min = readcoord(h, g, read);
    r.max = r.min;
    return r;
}

static geomCoord level1_getCenter(ghdr *h, uint8_t *g, int *read){
    return readcoord(h, g, read);
}

static geomRect level2_getBounds(ghdr *h, uint8_t *g, int *read){
	geomRect r;
	memset(&r, 0, sizeof(geomRect));
	int got = 0;
	uint8_t *p = g;
	int len = *((uint32_t*)p);
	int nread = 0;
	p += 4;
	for (int i=0;i<len;i++){
		geomCoord c = readcoord(h, p, &nread);
		if (!got){
			r.min = c;
			r.max = c;
			got = 1;
		} else {
			r = geomRectExpand(r, c);
		}
		p += nread;
	}
	if (!got){
		memset(&r, 0, sizeof(geomRect));
	}
	if (read){
		*read = p-g;
	}
    return r;
}

static geomCoord level2_getCenter(ghdr *h, uint8_t *g, int *read){
	return geomRectCenter(level2_getBounds(h, g, read));
}

static geomRect level3_getBounds(ghdr *h, uint8_t *g, int isPolygon, int *read){
	geomRect r;
	int got = 0;
	uint8_t *p = g;
	int len = *((uint32_t*)p);
	int nread = 0;
	p += 4;
	for (int i=0;i<len;i++){
		geomRect rr = level2_getBounds(h, p, &nread);
		if (!got){
			r = rr;
			got = 1;
		} else {			
			if (!isPolygon){
				// polygons only read the first/outer ring.
				r = geomRectUnion(r, rr);
			}
		}
		p += nread;
	}
	if (!got){
		memset(&r, 0, sizeof(geomRect));
	}
	if (read){
		*read = p-g;
	}
    return r;
}

static geomCoord level3_getCenter(ghdr *h, uint8_t *g, int isPolygon, int *read){
	return geomRectCenter(level3_getBounds(h, g, isPolygon, read));
}

static geomRect level4_getBounds(ghdr *h, uint8_t *g, int isPolygon, int *read){
	geomRect r;
	int got = 0;
	uint8_t *p = g;
	int len = *((uint32_t*)p);
	int nread = 0;
	p += 4;
	for (int i=0;i<len;i++){
		geomRect rr = level3_getBounds(h, p, isPolygon, &nread);
		if (!got){
			r = rr;
			got = 1;
		} else {
			r = geomRectUnion(r, rr);
		}
		p += nread;
	}
	if (!got){
		memset(&r, 0, sizeof(geomRect));
	}
	if (read){
		*read = p-g;
	}
    return r;
}

static geomCoord level4_getCenter(ghdr *h, uint8_t *g, int isPolygon, int *read){
	return geomRectCenter(level4_getBounds(h, g, isPolygon, read));
}

static geomRect gcol_getBounds(ghdr *h, uint8_t *g, int *read){
	geomRect r;
	int got = 0;
	uint8_t *p = g;
	int len = *((uint32_t*)p);
	int nread = 0;
	p += 4;
	for (int i=0;i<len;i++){
		geomRect rr = levelAny_geomBounds(p, &nread);
		if (nread == -1){
			memset(&r, 0, sizeof(geomRect));
			if (read){
				*read = -1;
			}
			return r;
		}
		if (!got){
			r = rr;
			got = 1;
		} else {
			r = geomRectUnion(r, rr);
		}
		p += nread;
	}
	if (!got){
		memset(&r, 0, sizeof(geomRect));
	}
	if (read){
		*read = p-g;
	}
    return r;
}

static geomCoord gcol_getCenter(ghdr *h, uint8_t *g, int *read){
	return geomRectCenter(gcol_getBounds(h, g, read));
}

static geomCoord levelAny_geomCenter(uint8_t *g, int *read){
    geomCoord c;
    uint8_t *p = g;
    p++; // skip endian byte
    ghdr h = readhdr(p);
    p+=4;
    int nread = 0;
    switch (h.type){
    default:{
        nread = -1; // error
        break;
    }   
    case GEOM_GEOMETRYCOLLECTION:
        c = gcol_getCenter(&h, p, NULL);
        break;
    case GEOM_MULTIPOLYGON:
        c = level4_getCenter(&h, p, 1, &nread);
        break;
    case GEOM_MULTILINESTRING:
        c = level3_getCenter(&h, p, 0, &nread);
        break;
    case GEOM_POLYGON:
        c = level3_getCenter(&h, p, 1, &nread);
        break;
    case GEOM_MULTIPOINT:
    case GEOM_LINESTRING:
        c = level2_getCenter(&h, p, &nread);
        break;
    case GEOM_POINT:
        c = level1_getCenter(&h, p, &nread);
        break;
    }
    if (nread == -1){
        memset(&c, 0, sizeof(geomCoord));
        if (read){
            *read = -1;
        }
    } else{
        p += nread;
        if (read){
            *read = p-g;
        }
    }
    return c;
}

static geomRect levelAny_geomBounds(uint8_t *g, int *read){
    geomRect r;
    uint8_t *p = g;
    p++; // skip endian byte
    ghdr h = readhdr(p);
    p+=4;
    int nread = 0;
    switch (h.type){
    default:{
        nread = -1; // error
        break;
    }   
    case GEOM_GEOMETRYCOLLECTION:
        r = gcol_getBounds(&h, p, &nread);
        break;
    case GEOM_MULTIPOLYGON:
        r = level4_getBounds(&h, p, 1, &nread);
        break;
    case GEOM_MULTILINESTRING:
        r = level3_getBounds(&h, p, 0, &nread);
        break;
    case GEOM_POLYGON:
        r = level3_getBounds(&h, p, 1, &nread);
        break;
    case GEOM_MULTIPOINT:
    case GEOM_LINESTRING:
        r = level2_getBounds(&h, p, &nread);
        break;
    case GEOM_POINT:
        r = level1_getBounds(&h, p, &nread);
        break;
    }
    if (nread == -1){
        memset(&r, 0, sizeof(geomRect));
        if (read){
            *read = -1;
        }
    } else{
        p += nread;
        if (read){
            *read = p-g;
        }
    }
    return r;
}


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

static int jsonDecodeAny(json_value *value, geom *g, int *size);

static uint8_t *makeBOID(int cap, int id, int dims){
	if (cap < 5){
		return NULL;
	}
    uint8_t *b = zmalloc(cap);
    if (!b){
        return NULL;
    }
    if (LITTLE_ENDIAN){
        b[0] = 0x01;
    } else{
        b[0] = 0x00;
    }
    switch (dims){
    case 2:
        *((uint32_t*)(b+1)) = id;
        break;
    case 3:
        *((uint32_t*)(b+1)) = 1000+id;
        break;    
    case 4:
        *((uint32_t*)(b+1)) = 3000+id;
        break;
    }
    return b;
}

static int jsonGetPosition(json_value *value, int *dims, geomCoord *position){
    if (!value||
        value->type!=json_array||
        value->u.array.length<2||
        value->u.array.length>4
    ){
        return 0;
    }
    geomCoord coord = {0,0,0,0};
    for (int i=0;i<value->u.array.length;i++){
        double n = 0;
        json_value *nvalue = value->u.array.values[i];
        if (nvalue->type==json_double){
            n = nvalue->u.dbl;
        } else if (nvalue->type==json_integer){
            n = nvalue->u.integer;
        } else {
            return 0;
        }
        switch (i){
        case 0:
            coord.x = n;
            break;
        case 1:
            coord.y = n;
            break;
        case 2:
            coord.z = n;
            break;
        case 4:
            coord.m = n;
            break;
        }
    }
    if (dims) *dims = value->u.array.length;
    if (position) *position = coord;
    return 1;
}

static int jsonDecodePoint(json_value *coords, geom *g, int *size){
	if (!coords||coords->type!=json_array){
		return 0;
	}
	int dims;
	geomCoord position;
	if (!jsonGetPosition(coords, &dims, &position)){
		return 0;
	}
	uint8_t *b = makeBOID(5+dims*8, 1, dims);
	if (!b){
		return 0;
	}
	((double*)(b+5))[0] = position.x;
	((double*)(b+5))[1] = position.y;
	if (dims>2){
		((double*)(b+5))[2] = position.z;
		if (dims>3){
			((double*)(b+5))[3] = position.m;
		}
	}
	if (g) *g = (geom)b;
	if (size) *size = 5+dims*8;
	return 1;
}

#define JD_BEGIN(type)\
	int rdims = 0;\
	int cap = 5;\
    uint8_t *b = zmalloc(cap);\
    if (!b){\
        goto err;\
    }\
    if (LITTLE_ENDIAN){\
        b[0] = 0x01;\
    } else{\
        b[0] = 0x00;\
    }\
    *((uint32_t*)(b+1)) = (type);\
    uint8_t *ptr = b+5;\
    rdims = rdims;

#define JD_EXPAND_BUFFER(n){\
    int ncap = cap+(n);\
    uint8_t *nb = zrealloc(b, ncap);\
    if (!nb){\
    	goto err;\
    }\
    ptr = nb+(ptr-b);\
    cap = ncap;\
    b = nb;\
}

#define JD_WRITE_INT(n)\
    JD_EXPAND_BUFFER(4);\
    *((uint32_t*)ptr) = (n);\
    ptr+=4;

#define JD_WRITE_DBL(d)\
    JD_EXPAND_BUFFER(8);\
    *((double*)ptr) = (d);\
    ptr+=8;

#define JD_END()\
    if (g) *g = (geom)b;\
    if (size) *size = ptr-b;\
    return 1;\
err:\
	if (b){\
		zfree(b);\
	}\
	return 0;


#define JD_WRITE_COORD(value){\
    int dims;\
	geomCoord position;\
	if (!jsonGetPosition((value), &dims, &position)){\
		goto err;\
	}\
	if (rdims==0){\
		rdims=dims;\
		if (rdims!=2){\
			if (rdims==3){\
				*((uint32_t*)(b+1)) += 1000;\
			} else if (rdims==4){\
				*((uint32_t*)(b+1)) += 3000;\
			}\
		}\
	} else if (dims!=rdims){\
		goto err;\
	}\
	JD_WRITE_DBL(position.x);\
	JD_WRITE_DBL(position.y);\
	if (dims>2){\
		JD_WRITE_DBL(position.z);\
		if (dims>3){\
			JD_WRITE_DBL(position.m);\
		}\
	}\
}

#define JD_WRITE_MULTI1_COORDS(coords){\
	if (!(coords)||(coords)->type!=json_array){\
		goto err;\
	}\
	JD_WRITE_INT((coords)->u.array.length);\
	for (int i=0;i<(coords)->u.array.length;i++){\
		json_value *nvalue1 = (coords)->u.array.values[i];\
		JD_WRITE_COORD(nvalue1);\
	}\
}

#define JD_WRITE_MULTI2_COORDS(coords){\
	if (!(coords)||(coords)->type!=json_array){\
		goto err;\
	}\
	JD_WRITE_INT((coords)->u.array.length);\
	for (int i=0;i<(coords)->u.array.length;i++){\
		json_value *nvalue2 = (coords)->u.array.values[i];\
		JD_WRITE_MULTI1_COORDS(nvalue2);\
	}\
}

#define JD_WRITE_MULTI3_COORDS(coords){\
	if (!(coords)||(coords)->type!=json_array){\
		goto err;\
	}\
	JD_WRITE_INT((coords)->u.array.length);\
	for (int i=0;i<(coords)->u.array.length;i++){\
		json_value *nvalue3 = (coords)->u.array.values[i];\
		JD_WRITE_MULTI2_COORDS(nvalue3);\
	}\
}


static int jsonDecodeLineString(json_value *coords, geom *g, int *size){
	JD_BEGIN(2);
	JD_WRITE_MULTI1_COORDS(coords);
    JD_END();
}
static int jsonDecodePolygon(json_value *coords, geom *g, int *size){
	JD_BEGIN(3);
	JD_WRITE_MULTI2_COORDS(coords);
    JD_END();
}
static int jsonDecodeMultiPoint(json_value *coords, geom *g, int *size){
	JD_BEGIN(4);
	JD_WRITE_MULTI1_COORDS(coords);
    JD_END();
}
static int jsonDecodeMultiLineString(json_value *coords, geom *g, int *size){
	JD_BEGIN(5);
	JD_WRITE_MULTI2_COORDS(coords);
    JD_END();
}
static int jsonDecodeMultiPolygon(json_value *coords, geom *g, int *size){
	JD_BEGIN(6);
	JD_WRITE_MULTI3_COORDS(coords);
    JD_END();
}

static int jsonDecodeGeometryCollection(json_value *geometries, geom *g, int *size){
	JD_BEGIN(7);

	if (!(geometries)||(geometries)->type!=json_array){
		goto err;
	}
	JD_WRITE_INT((geometries)->u.array.length);
	for (int i=0;i<(geometries)->u.array.length;i++){
		geom ng;
		int nsize;
		if (!jsonDecodeAny((geometries)->u.array.values[i], &ng, &nsize)){
			goto err;
		}
		JD_EXPAND_BUFFER(nsize);
		memcpy(ptr, (uint8_t*)ng, nsize);
		ptr+=nsize;
		geomFree(ng);
	}

	JD_END();
}

static int jsonDecodeAny(json_value *value, geom *g, int *size){
	if (!value|| 
		value->type != json_object){
		return 0;
	}
	char *type = "";
    json_value *coordinates = NULL;
    json_value *geometries = NULL;
    for (int i=0;i<value->u.object.length;i++){
        if (!strcmp(value->u.object.values[i].name, "type")){
            if (value->u.object.values[i].value->type == json_string){
                type = value->u.object.values[i].value->u.string.ptr;
            }
        }
        if (!strcmp(value->u.object.values[i].name, "coordinates")){
            if (value->u.object.values[i].value->type == json_array){
                coordinates = value->u.object.values[i].value;
            }
        }
        if (!strcmp(value->u.object.values[i].name, "geometries")){
            if (value->u.object.values[i].value->type == json_array){
                geometries = value->u.object.values[i].value;
            }
        }
    }
    if (!strcmp(type, "Point")){
        return jsonDecodePoint(coordinates, g, size);
    } else if (!strcmp(type, "MultiPoint")){
        return jsonDecodeMultiPoint(coordinates, g, size);
    } else if (!strcmp(type, "LineString")){
        return jsonDecodeLineString(coordinates, g, size);
    } else if (!strcmp(type, "MultiLineString")){
        return jsonDecodeMultiLineString(coordinates, g, size);
    } else if (!strcmp(type, "Polygon")){ 
        return jsonDecodePolygon(coordinates, g, size);
    } else if (!strcmp(type, "MultiPolygon")){ 
        return jsonDecodeMultiPolygon(coordinates, g, size);
    } else if (!strcmp(type, "GeometryCollection")){
        return jsonDecodeGeometryCollection(geometries, g, size);
    } 
    return 0;
}




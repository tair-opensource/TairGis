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


void geomFreePolyMap(geomPolyMap *m){
	if (m) {
		if (m->geoms && m->collection){
			geomFreeFlattenedArray(m->geoms);
		}
		if (m->polygons && m->multipoly){
			zfree(m->polygons);
		}
		if (m->holes && m->multipoly){
			zfree(m->holes);
		}
		if (m->types) {
            zfree(m->types);
		}
		zfree(m);
	}
}

static geomPolyMap *geomNewPolyMapBase(geom g){
	if (!g){
		return NULL;
	}
	uint8_t *ptr = (uint8_t*)g;
	if (*ptr!=0&&*ptr!=1){
		return NULL;
	}
	ptr++;
	ghdr h = readhdr(ptr);
	ptr+=4;
	if (h.type == GEOM_UNKNOWN){
		return NULL;
	}
	int dims = 2;
	if (h.z){
		dims++;
	}
	if (h.m){
		dims++;
	}
	geomPolyMap *m = zmalloc(sizeof(geomPolyMap));
    if (!m) {
        return NULL;
    }
    memset(m, 0, sizeof(geomPolyMap));
	
	m->g = g;
	geomCoord center = geomCenter(g);
	m->center.x = center.x;
	m->center.y = center.y;
	geomRect bounds = geomBounds(g);
	m->bounds.min.x = bounds.min.x;
	m->bounds.min.y = bounds.min.y;
	m->bounds.max.x = bounds.max.x;
	m->bounds.max.y = bounds.max.y;
	if (h.type==GEOM_GEOMETRYCOLLECTION){
		m->collection = 1;
		m->geoms = geomGeometryCollectionFlattenedArray(g, &m->geomCount);
		if (!m->geoms){
			goto err;
		}
	} else {
		m->geoms = &g;
		m->geomCount = 1;
	}
	m->type = h.type;
	m->z = h.z;
	m->m = h.m;
	m->dims = dims;

	switch (h.type){
	case GEOM_POINT:	
	case GEOM_LINESTRING:
		switch (h.type){
		case GEOM_POINT:
			m->ppoly.len = 1;
			break;
		case GEOM_LINESTRING:
			m->ppoly.len = *((uint32_t*)ptr);
			ptr+=4;
			break;
		}
		m->ppoly.dims = dims;
		m->ppoly.values = (double*)ptr;
		m->polygonCount = 1;
		m->polygons = &m->ppoly;
		m->holes = &m->pholes;
		m->types = zmalloc(sizeof(geomType));
		if (!m->types){
		    goto err;
		}
		*(m->types) = m->type;
		break;
	case GEOM_POLYGON: 
	single_polygon:{
		int count = *((uint32_t*)ptr);
		ptr+=4;
		if (count>0){
			m->ppoly.len = *((uint32_t*)ptr);
			ptr+=4;
			m->ppoly.dims = dims;
			m->ppoly.values = (double*)ptr;
			ptr+=dims*8*m->ppoly.len;
			if (count>1){
				m->pholes.len = count-1;
				m->pholes.dims = dims;
				m->pholes.values = (double*)ptr;
			}
		} 
		m->polygonCount = 1;
		m->polygons = &m->ppoly;
		m->holes = &m->pholes;
        m->types = zmalloc(sizeof(geomType));
        if (!m->types){
            goto err;
        }
        *(m->types) = m->type;
		break;
	}
	case GEOM_MULTIPOINT:{
		int count = *((uint32_t*)ptr);
		ptr+=4;
		m->multipoly = 1;
		m->polygonCount = count;
		m->polygons = zmalloc(count*sizeof(polyPolygon));
		if (!m->polygons){
			goto err;
		}
		memset(m->polygons, 0, count*sizeof(polyPolygon));
		m->holes = zmalloc(count*sizeof(polyMultiPolygon));
		if (!m->holes){
			goto err;
		}
		memset(m->holes, 0, count*sizeof(polyMultiPolygon));
		m->types = zmalloc(count*sizeof(geomType));
		if (!m->types){
			goto err;
		}
		memset(m->types, 0, count*sizeof(geomType));
		for (int i=0;i<count;i++){
			m->polygons[i].len = 1;
			m->polygons[i].dims = dims;
			m->polygons[i].values = (double*)ptr;
			ptr+=dims*8;
			m->types[i] = GEOM_POINT; // reduce to a point
		}
		break;
	}
	case GEOM_MULTILINESTRING:{
		int count = *((uint32_t*)ptr);
		ptr+=4;
		m->multipoly = 1;
		m->polygonCount = count;
		m->polygons = zmalloc(count*sizeof(polyPolygon));
		if (!m->polygons){
			goto err;
		}
		memset(m->polygons, 0, count*sizeof(polyPolygon));
		m->holes = zmalloc(count*sizeof(polyMultiPolygon));
		if (!m->holes){
			goto err;
		}
		memset(m->holes, 0, count*sizeof(polyMultiPolygon));
		m->types = zmalloc(count*sizeof(geomType));
		if (!m->types){
			goto err;
		}
		memset(m->types, 0, count*sizeof(geomType));
		for (int i=0;i<count;i++){
			m->polygons[i].len = *((uint32_t*)ptr);
			ptr+=4;
			m->polygons[i].dims = dims;
			m->polygons[i].values = (double*)ptr;
			ptr+=dims*8*m->polygons[i].len;
			m->types[i] = GEOM_LINESTRING; // reduce to a linestring
		}
		break;
	}
	case GEOM_MULTIPOLYGON:{
		int count = *((uint32_t*)ptr);
		if (count<=1){
			goto single_polygon;
		}
		ptr+=4;
		m->multipoly = 1;
		m->polygonCount = count;
		m->polygons = zmalloc(count*sizeof(polyPolygon));
		if (!m->polygons){
			goto err;
		}
		memset(m->polygons, 0, count*sizeof(polyPolygon));
		m->holes = zmalloc(count*sizeof(polyMultiPolygon));
		if (!m->holes){
			goto err;
		}
		memset(m->holes, 0, count*sizeof(polyMultiPolygon));
		m->types = zmalloc(count*sizeof(geomType));
		if (!m->types){
			goto err;
		}
		memset(m->types, 0, count*sizeof(geomType));
		for (int i=0;i<count;i++){
			int count2 = *((uint32_t*)ptr);
			ptr+=4;
			if (count2==0){
				continue;
			}
			m->polygons[i].len = *((uint32_t*)ptr);
			ptr+=4;
			m->polygons[i].dims = dims;
			m->polygons[i].values = (double*)ptr;
			ptr+=dims*8*m->polygons[i].len;	
			if (count2>1){
				m->holes[i].len = count2-1;
				m->holes[i].dims = dims;
				m->holes[i].values = (double*)ptr;
				for (int j=1;j<count2;j++){
					int count3 = *((uint32_t*)ptr);
					ptr+=4;
					ptr+=dims*8*count3;
				}
			}
			m->types[i] = GEOM_POLYGON; // reduce to a polygon
		}
		break;
	}
	case GEOM_GEOMETRYCOLLECTION:{
		int cap = 1;
		int len = 0;
		polyPolygon *polygons = NULL;
		polyMultiPolygon *holes = NULL;
		geomType *types = NULL;
		geomPolyMap *m2 = NULL;
		polygons = zmalloc(cap*sizeof(polyPolygon));
		if (!polygons){
			goto err2;
		}
		holes = zmalloc(cap*sizeof(polyMultiPolygon));
		if (!holes){
			goto err2;
		}
		types = zmalloc(cap*sizeof(geomType));
		if (!types){
			goto err2;
		}
		for (int i=0;i<m->geomCount;i++){
			geom g2 = m->geoms[i];
			m2 = geomNewPolyMap(g2);
			if (!m2){
				goto err2;
			}
			for (int j=0;j<m2->polygonCount;j++){
				if (len==cap){
					int ncap = cap==0?1:cap*2;
					polyPolygon *npolygons = zrealloc(polygons, ncap*sizeof(polyPolygon));
					if (!npolygons){
						goto err2;
					}
					polyMultiPolygon *nholes = zrealloc(holes, ncap*sizeof(polyMultiPolygon));
					if (!nholes){
						goto err2;
					}
					geomType *ntypes = zrealloc(types, ncap*sizeof(geomType));
					if (!ntypes){
						goto err2;
					}
					polygons = npolygons;
					holes = nholes;
					types = ntypes;
					cap = ncap;
				}
				polygons[len] = m2->polygons[j];
				holes[len] = m2->holes[j];
				types[len] = m2->types[j];
				len++;
			}
			geomFreePolyMap(m2);	
			m2 = NULL;
		}
		m->multipoly = 1;
		m->polygonCount = len;

		m->polygons = polygons;
		m->holes = holes;
		m->types = types;
		break;
	err2:
		if (polygons){
			zfree(polygons);
		}
		if (holes){
			zfree(holes);
		}
		if (types){
			zfree(types);
		}
		if (m2){
			geomFreePolyMap(m2);
		}
		goto err;
	}}
	return m;
err:
	if (m){
		geomFreePolyMap(m);
	}
	return NULL;
}

geomPolyMap *geomNewPolyMap(geom g){
	return geomNewPolyMapBase(g);
}

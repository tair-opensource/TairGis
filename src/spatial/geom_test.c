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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include "zmalloc.h"
#include "test.h"
#include "geom.h"

char *randgeometrycollection(int vary, int dim);

// parseSerializeAndCompare parses, serializes, parses again, serializes again
// and compares the the first and second serialized strings.
void parseSerializeAndCompare(const char *input){
    geom g = NULL;
    int sz = 0;
    geomErr err = geomDecode(input, strlen(input), 0, &g, &sz);
    assert(err == GEOM_ERR_NONE);
    char *text1 = geomEncodeWKT(g, 0);
    assert(text1 != NULL);
    geom g2 = NULL;
    int sz2 = 0;
    err = geomDecode(g, sz, 0, &g2, &sz2);
    assert(err == GEOM_ERR_NONE);
    assert(sz == sz2);
    assert(memcmp(g, g2, sz) == 0);
    geomFree(g);
    geomFree(g2);
    err = geomDecode(input, strlen(input), 0, &g, &sz);
    assert(err == GEOM_ERR_NONE);
    char *text2 = geomEncodeWKT(g, 0);
    assert(text2 != NULL);
    geomFree(g);
    assert(strcmp(text1, text2)==0);
    zfree(text1);
    zfree(text2);
}

// dstr converts a double to a string and keeps much of the precision.
// This is quick but not anything like the Grisu algorightm.
char *dstr(double n, char *str){
    sprintf(str, "%.15f", n);
    char *p = str+strlen(str)-1;
    while (*p=='0'&&p>str){
        *p = 0;
        p--;
    }
    if (*p == '.'){
        *p = 0;
    }
    return str;
}

// randd creates a random double [1-0].
double randd(){
    return ((rand()%RAND_MAX) / (double)RAND_MAX);
}

// randx create a random longitude.
double randx() {
    return randd() * 360.0 - 180.0;
}

// randy create a random latitude.
double randy() {
    return randd() * 180.0 - 90.0;
}

#define VINIT()\
    char *str = zmalloc(257);\
    int sz = 256;\
    int idx = 0;\
    str[0] = 0;

#define VCHAR(ch){\
    char cch = ch;\
    if (idx == sz){\
        sz *= 2;\
        str = zrealloc(str, sz+1);\
        assert(str);\
    }\
    if (vary && cch == ' '){\
        switch (rand()%6){\
        case 0: cch = '\t'; break;\
        case 1: cch = ' '; break;\
        case 2: cch = '\r'; break;\
        case 3: cch = '\v'; break;\
        case 4: cch = '\n'; break;\
        case 5: cch = '\f'; break;\
        }\
    }\
    str[idx++] = cch;\
    str[idx] = 0;\
}

#define VSPACES()\
    if (vary){\
        int n = rand() % 5;\
        for (int i=0;i<n;i++){\
            VCHAR(' ');\
        }\
    }

#define VSTR(sstr){\
    VSPACES();\
    char *p = sstr;\
    while (*p){\
        char ch = *p;\
        if (vary){\
            if (rand()%2==0){\
                ch = tolower(ch);\
            } else{\
                ch = toupper(ch);\
            }\
        }\
        VCHAR(ch);\
        p++;\
    }\
    VSPACES();\
}

#define VPOINT(dim){\
    char *pstr = randpoint(vary, (dim));\
    VSTR(pstr);\
    zfree(pstr);\
}

#define VLINESTRING(dim){\
    char *pstr = randlinestring(vary, (dim));\
    VSTR(pstr);\
    zfree(pstr);\
}

#define VPOLYGON(dim){\
    char *pstr = randpolygon(vary, (dim));\
    VSTR(pstr);\
    zfree(pstr);\
}

#define VMULTIPOLYGON(dim){\
    char *pstr = randmultipolygon(vary, (dim));\
    VSTR(pstr);\
    zfree(pstr);\
}

#define VGEOMETRYCOLLECTION(dim){\
    char *pstr = randgeometrycollection(vary, (dim));\
    VSTR(pstr);\
    zfree(pstr);\
}


// randx create a random string lat lon in XY format.
char *randpoint(int vary, int dim){
    char output[50];
    VINIT();
    for (int i=0;i<dim;i++){
        if (i!=0){
            VCHAR(' ');     
        }
        dstr(randx(), output);
        VSTR(output);   
    }
    return str;
}


char *randlinestring(int vary, int dim){
    VINIT();
    int n = 5;
    if (vary){
        n = rand() % 25;
    }
    for (int i=0;i<n;i++){
        if (i!=0){
            VCHAR(',');
        }
        VPOINT(2);
    }
    return str;
}


char *randpolygon(int vary, int dim){
    VINIT();
    int n = 3;
    if (vary){
        n = (rand() % 10) + 1;
    }
    for (int i=0;i<n;i++){
        if (i!=0){
            VCHAR(',');
        }
        VCHAR('('); 
        VLINESTRING(2);
        VCHAR(')');
    }
    return str;
}

char *randmultipolygon(int vary, int dim){
    VINIT();
    int n = 3;
    if (vary){
        n = (rand() % 10) + 1;
    }
    for (int i=0;i<n;i++){
        if (i!=0){
            VCHAR(',');
        }
        VCHAR('('); 
        VPOLYGON(2);
        VCHAR(')');
    }
    return str;
}

// randGeomPoint creates a random POINT(1 2) geometry string.
char *randGeomPoint(int vary, int dim){
    VINIT();
    VSTR("POINT");
    VCHAR('(');
    VPOINT(dim);
    VCHAR(')');
    return str; 
}

// randGeomLineString creates a random LINESTRING(1 2, 3 4, 5 6) geometry string.
char *randGeomLineString(int vary, int dim){
    VINIT();
    VSTR("LINESTRING");
    VCHAR('(');
    VLINESTRING(dim);
    VCHAR(')');
    return str; 
}

char *randGeomMultiPoint(int vary, int dim){
    VINIT();
    VSTR("MULTIPOINT");
    VCHAR('(');
    VLINESTRING(dim);
    VCHAR(')');
    return str; 
}

char *randGeomPolygon(int vary, int dim){
    VINIT();
    VSTR("POLYGON");
    VCHAR('(');
    VPOLYGON(dim);
    VCHAR(')');
    return str; 
}

char *randGeomMultiLineString(int vary, int dim){
    VINIT();
    VSTR("MULTILINESTRING");
    VCHAR('(');
    VPOLYGON(dim);
    VCHAR(')');
    return str; 
}

char *randGeomMultiPolygon(int vary, int dim){
    VINIT();
    VSTR("MULTIPOLYGON");
    VCHAR('(');
    VMULTIPOLYGON(dim);
    VCHAR(')');
    return str; 
}

char *randGeomGeometryCollection(int vary, int dim){
    VINIT();
    VSTR("GEOMETRYCOLLECTION");
    VCHAR('(');
    VGEOMETRYCOLLECTION(dim);
    VCHAR(')');
    return str; 
}


char *randgeometrycollection(int vary, int dim){
    VINIT();
    int n = 3;
    if (vary){
        n = (rand() % 10) + 1;
    }
    for (int i=0;i<n;i++){
        if (i!=0){
            VCHAR(',');
        }
        char *sstr = NULL;
        switch (rand() % 6){
        case 0: sstr = randGeomPoint(vary, dim); break;
        case 1: sstr = randGeomLineString(vary, dim); break;
        case 2: sstr = randGeomMultiPoint(vary, dim); break;
        case 3: sstr = randGeomPolygon(vary, dim); break;
        case 4: sstr = randGeomMultiLineString(vary, dim); break;
        case 5: sstr = randGeomMultiPolygon(vary, dim); break;
        }
        VSTR(sstr);
        zfree(sstr);
    }
    return str;
}

void testGeom(int count, int dim, char *(*randGeomCreate)(int, int)){
    for (int i=0;i<count;i++){
        char *tstr = randGeomCreate(0, dim);
        parseSerializeAndCompare(tstr);
        zfree(tstr);
    }
    for (int i=0;i<count;i++){
        char *tstr = randGeomCreate(1, dim);
        parseSerializeAndCompare(tstr);
        zfree(tstr);
    }
}

int test_Geom(){
    srand(time(NULL)/clock());
    testGeom(400, 2, randGeomPoint);
    testGeom(300, 2, randGeomLineString);
    testGeom(300, 2, randGeomMultiPoint);
    testGeom(200, 2, randGeomPolygon);
    testGeom(200, 2, randGeomMultiLineString);
    testGeom(100, 2, randGeomMultiPolygon);
    testGeom(50,  2, randGeomGeometryCollection);
    return 1;
}

int test_GeomZ(){
    srand(time(NULL)/clock());
    testGeom(400, 3, randGeomPoint);
    testGeom(300, 3, randGeomLineString);
    testGeom(300, 3, randGeomMultiPoint);
    testGeom(200, 3, randGeomPolygon);
    testGeom(200, 3, randGeomMultiLineString);
    testGeom(100, 3, randGeomMultiPolygon);
    testGeom(50,  3, randGeomGeometryCollection);
    return 1;
}

int test_GeomZM(){
    srand(time(NULL)/clock());
    testGeom(400, 4, randGeomPoint);
    testGeom(300, 4, randGeomLineString);
    testGeom(300, 4, randGeomMultiPoint);
    testGeom(200, 4, randGeomPolygon);
    testGeom(200, 4, randGeomMultiLineString);
    testGeom(100, 4, randGeomMultiPolygon);
    testGeom(50,  4, randGeomGeometryCollection);
    return 1;
}

static void testGeomPointWKT(char *wkt, double x, double y, double z, double m){
    geom g = NULL;
    int sz = 0;
    geomErr err = geomDecodeWKT(wkt, GEOM_WKT_REQUIRE_ZM, &g, &sz);
    assert(err == GEOM_ERR_NONE);
    geomCoord p = geomCenter(g);
    assert(p.x==x);
    assert(p.y==y);
    assert(p.z==z);
    assert(p.m==m);
    geomRect r = geomBounds(g);
    assert(r.min.x==x);
    assert(r.min.y==y);
    assert(r.min.z==z);
    assert(r.min.m==m);
    assert(r.max.x==x);
    assert(r.max.y==y);
    assert(r.max.z==z);
    assert(r.max.m==m);
    geomFree(g);
}

int test_GeomPoint(){
    testGeomPointWKT("POINTZ(10 11 12)", 10, 11, 12, 0);
    testGeomPointWKT("POINT Z(10 11 12)", 10, 11, 12, 0);
    testGeomPointWKT("POINT ZM(10 11 12 13)", 10, 11, 12, 13);
    testGeomPointWKT("POINTZM(10 11 12 13)", 10, 11, 12, 13);
    return 1;
}


static void testGeomMBRWKT(char *wkt, char *tstr){
    geom g = NULL;
    int sz = 0;
    geomErr err = geomDecodeWKT(wkt, 0, &g, &sz);
    assert(err == GEOM_ERR_NONE);
    geomRect r = geomBounds(g);
    char str[250];
    geomRectString(r, 1, 1, str);
    assert(strcmp(tstr, str)==0);
    geomFree(g);
}


int test_GeomMultiPoint(){
    testGeomMBRWKT("MULTIPOINT(10 11, 12 13, 14 15)", "10 11 0 0,14 15 0 0");
    testGeomMBRWKT("MULTIPOINTZ(10 11 9,12 13 8,14 15 7)", "10 11 7 0,14 15 9 0");
    testGeomMBRWKT("MULTIPOINT Z(10 11 9,12 13 8,14 15 7)", "10 11 7 0,14 15 9 0");
    testGeomMBRWKT("MULTIPOINT ZM(10 11 9 100,12 13 8 101,14 15 7 102)", "10 11 7 100,14 15 9 102");
    testGeomMBRWKT("MULTIPOINT(10 11 9 100,12 13 8 101,14 15 7 102)", "10 11 7 100,14 15 9 102");
    testGeomMBRWKT("MULTIPOINTM(10 11 100,12 13 101,14 15 102)", "10 11 0 100,14 15 0 102");
    return 1;
}

int test_GeomLineString(){
    testGeomMBRWKT("LINESTRING(10 11, 12 13, 14 15)", "10 11 0 0,14 15 0 0");
    testGeomMBRWKT("LINESTRINGZ(10 11 9,12 13 8,14 15 7)", "10 11 7 0,14 15 9 0");
    testGeomMBRWKT("LINESTRING Z(10 11 9,12 13 8,14 15 7)", "10 11 7 0,14 15 9 0");
    testGeomMBRWKT("LINESTRING ZM(10 11 9 100,12 13 8 101,14 15 7 102)", "10 11 7 100,14 15 9 102");
    testGeomMBRWKT("LINESTRING(10 11 9 100,12 13 8 101,14 15 7 102)", "10 11 7 100,14 15 9 102");
    testGeomMBRWKT("LINESTRINGM(10 11 100,12 13 101,14 15 102)", "10 11 0 100,14 15 0 102");
    return 1;
}

int test_GeomPolygon(){
    testGeomMBRWKT("POLYGON((10 11, 12 13, 14 15),(9 8, 12 13))", "10 11 0 0,14 15 0 0");
    return 1;
}

int test_GeomMultiLineString(){
    testGeomMBRWKT("MULTILINESTRING((10 11, 12 13, 14 15),(9 8, 12 13))", "9 8 0 0,14 15 0 0");
    return 1;
}

int test_GeomMultiPolygon(){
    testGeomMBRWKT("MULTIPOLYGON(((10 11, 12 13, 14 15),(9 8, 12 13)))", "10 11 0 0,14 15 0 0");
    testGeomMBRWKT("MULTIPOLYGON("
            "((10 11, 12 13, 14 15),(9 8, 12 13)),"
            "((9 11, 12 13, 14 15),(9 8, 12 13))"
        ")", "9 11 0 0,14 15 0 0");
    return 1;
}



int test_GeomGeometryCollection(){
    testGeomMBRWKT("GEOMETRYCOLLECTION EMPTY", "0 0 0 0,0 0 0 0");
    testGeomMBRWKT("GEOMETRYCOLLECTION ()", "0 0 0 0,0 0 0 0");
    testGeomMBRWKT("GEOMETRYCOLLECTION ("
        "MULTIPOINT(10 11, 12 13, 14 15)"
    ")", "10 11 0 0,14 15 0 0");
    testGeomMBRWKT("GEOMETRYCOLLECTION ("
        "MULTIPOINT(10 11, 12 13, 14 15),"
        "POLYGON((101 111, 121 131, 141 151),(9 8, 12 13))"
    ")", "10 11 0 0,141 151 0 0");
    return 1;
}

int polyMapBench(char *input, int singleThreaded){
    geom g;
    int sz;
    geomErr err = geomDecode(input, strlen(input), 0, &g, &sz);
    assert(err == GEOM_ERR_NONE);
    int n = 50000;
    for (int i=0;i<n;i++){
        geomPolyMap *m = NULL;
        if (singleThreaded){
            m = geomNewPolyMapSingleThreaded(g);
        } else {
            m = geomNewPolyMap(g);
        }
        assert(m);
        geomFreePolyMap(m);
    }
    geomFree(g);
    return n;
}


static int test_GeomPolyMapPointBenchBase(int singleThreaded){
    return polyMapBench("POINT(10 11)", singleThreaded);
}
static int test_GeomPolyMapPolygonBenchBase(int singleThreaded){
    return polyMapBench("POLYGON((10 11, 12 13, 14 15,16 17,10 11),(9 8, 12 13, 9 8))", singleThreaded);
}
static int test_GeomPolyMapGeometryCollectionBenchBase(int singleThreaded){
    char *input = 
        "GEOMETRYCOLLECTION ("
    /*  1 */         "POINT Z(10 11 12),"
    /*  2 */         "MULTIPOINT(10 11, 12 13, 14 15),"
    /*  3 */         "POLYGON((101 111, 121 131, 141 151),(9 8, 12 13)),"
    /*  4 */         "POINTZM(10 11 12 13),"
    /*  5 */         "POLYGON((10 11, 12 13, 14 15),(9 8, 12 13)),"
    /*    */         "GEOMETRYCOLLECTION ("
    /*  6 */             "MULTIPOINT(10 11, 12 13, 14 15),"
    /*  7 */             "POLYGON((101 111, 121 131, 141 151),(9 8, 12 13))"
    /*    */         "),"
    /*  8 */         "POINTZ(10 11 12),"
    /*  9 */         "LINESTRINGZ(10 11 9,12 13 8,14 15 7),"
    /* 10 */         "LINESTRING ZM(10 11 9 100,12 13 8 101,14 15 7 102),"
    /* 11 */         "POINT ZM(10 11 12 13)"
        ")";
    return polyMapBench(input, singleThreaded);
}
int test_GeomPolyMapPointBenchSingleThreaded(){
    return test_GeomPolyMapPointBenchBase(1);
}
int test_GeomPolyMapPolygonBenchSingleThreaded(){
    return test_GeomPolyMapPolygonBenchBase(1);
}
int test_GeomPolyMapGeometryCollectionBenchSingleThreaded(){
    return test_GeomPolyMapGeometryCollectionBenchBase(1);
}
int test_GeomPolyMapPointBench(){
    return test_GeomPolyMapPointBenchBase(0);
}
int test_GeomPolyMapPolygonBench(){
    return test_GeomPolyMapPolygonBenchBase(0);
}
int test_GeomPolyMapGeometryCollectionBench(){
    return test_GeomPolyMapGeometryCollectionBenchBase(0);
}

int test_GeomPolyMap(){
    char *input = 
        "GEOMETRYCOLLECTION ("
    /*  1 */    "POINT Z(10 11 12),"
    /*  2 */    "MULTIPOINT(10 11, 12 13, 14 15),"
    /*  3 */    "POLYGON((101 111, 121 131, 141 151),(9 8, 12 13)),"
    /*  4 */    "POINTZM(10 11 12 13),"
    /*  5 */    "POLYGON((10 11, 12 13, 14 15),(9 8, 12 13)),"
    /*  6 */    "MULTIPOLYGON(((10 11, 12 13, 14 15),(9 8, 12 13))),"
    /*  7 */    "MULTIPOINT(10 11, 12 13, 14 15),"
    /*  8 */    "POLYGON((101 111, 121 131, 141 151)),"
    /*    */    "GEOMETRYCOLLECTION ("
    /*  9 */        "MULTIPOLYGON(((10 11, 12 13, 14 15),(9 8, 12 13))),"
    /* 10 */        "MULTIPOINT(10 11, 12 13, 14 15),"
    /*    */        "GEOMETRYCOLLECTION ("
    /* 11 */            "MULTIPOLYGON(((10 11, 12 13, 14 15),(9 8, 12 13))),"
    /* 12 */            "MULTIPOINT(10 11, 12 13, 14 15),"
    /* 13 */            "POLYGON((101 111, 121 131, 141 151)),"
    /* 14 */            "MULTIPOINT(10 11, 12 13, 14 15)"
    /*    */        "),"
    /* 15 */        "MULTIPOINT(10 11, 12 13, 14 15)"
    /*    */    "),"
    /* 16 */    "POINTZ(10 11 12),"
    /* 17 */    "MULTIPOLYGON(((10 11, 12 13, 14 15),(9 8, 12 13))),"
    /* 18 */    "LINESTRINGZ(10 11 9,12 13 8,14 15 7),"
    /* 19 */    "LINESTRING ZM(10 11 9 100,12 13 8 101,14 15 7 102),"
    /* 20 */    "MULTIPOLYGON EMPTY,"
    /* 21 */    "MULTIPOLYGON(((10 11, 12 13, 14 15),(9 8, 12 13)),((9 8, 12 13))),"
    /* 22 */    "POINT ZM(10 11 12 13)"
        ")";
    
    geom g;
    int sz;

    // for (int i=0;;i++){
    //     if (i==1000000){
    //         return i;
    //     }
        geomErr err = geomDecode(input, strlen(input), 0, &g, &sz);
        assert(err == GEOM_ERR_NONE);


        geomPolyMap *m = geomNewPolyMap(g);
        assert(m);

        assert(m->geomCount==22);
        assert(m->polygonCount==35);

        geomFreePolyMap(m);
        geomFree(g);
    // }
    return 1;
}

int test_GeomIterator(){
    char *input = 
        "GEOMETRYCOLLECTION ("
    /*  1 */         "POINT Z(10 11 12),"
    /*  2 */         "MULTIPOINT(10 11, 12 13, 14 15),"
    /*  3 */         "POLYGON((101 111, 121 131, 141 151),(9 8, 12 13)),"
    /*  4 */         "POINTZM(10 11 12 13),"
    /*  5 */         "POLYGON((10 11, 12 13, 14 15),(9 8, 12 13)),"
    /*    */         "GEOMETRYCOLLECTION ("
    /*  6 */             "MULTIPOINT(10 11, 12 13, 14 15),"
    /*  7 */             "POLYGON((101 111, 121 131, 141 151),(9 8, 12 13))"
    /*    */         "),"
    /*  8 */         "POINTZ(10 11 12),"
    /*  9 */         "LINESTRINGZ(10 11 9,12 13 8,14 15 7),"
    /* 10 */         "LINESTRING ZM(10 11 9 100,12 13 8 101,14 15 7 102),"
    /* 11 */         "POINT ZM(10 11 12 13)"
        ")";

    geom g;
    int sz;
    geomErr err = geomDecode(input, strlen(input), 0, &g, &sz);
    assert(err == GEOM_ERR_NONE);
    int count = 0;
    geomIterator itr;
    err = geomGeometryCollectionIterator(g, &itr);
    assert(err == GEOM_ERR_NONE);
    while (geomIteratorNext(&itr)){
        geom ig;
        int sz;
        assert(geomIteratorValues(&itr, &ig, &sz));
        if (geomGetType(ig) == GEOM_GEOMETRYCOLLECTION){
            geomIterator itr2;
            err = geomGeometryCollectionIterator(ig, &itr2);
            assert(err==GEOM_ERR_NONE);
            while (geomIteratorNext(&itr2)){
                geom ig2;
                int sz2;
                assert(geomIteratorValues(&itr2, &ig2, &sz2));
                count++;
            }
        }
        count++;
    }
    int count2;
    geom *arr = geomGeometryCollectionFlattenedArray(g, &count2);
    assert(arr);
    assert(count2==11);
    geomFreeFlattenedArray(arr);
    geomFree(g);
    return 1;
}

static geom decode(char *wkt){
    geom g;
    int sz;
    geomErr err = geomDecodeWKT(wkt, 0, &g, &sz);
    assert(err==GEOM_ERR_NONE);
    return g;
}

static int testIntersects(char *input1, char *input2){
    geom g1 = decode(input1);
    geom g2 = decode(input2);
    assert(g1 && g2);
    geomPolyMap *m1 = geomNewPolyMap(g1);
    geomPolyMap *m2 = geomNewPolyMap(g2);
    assert(m1 && m2);
    int res = geomPolyMapIntersects(m2, m1);
    geomFreePolyMap(m1);
    geomFreePolyMap(m2);
    geomFree(g1);
    geomFree(g2);
    return res;
}

int test_GeomPolyMapIntersects(){
    assert(testIntersects(
        "POLYGON((0 0, 0 16, 16 16, 16 0, 0 0))",
        "POLYGON((15 15, 15 20, 20 20, 20 15, 15 15))"
    ));
    assert(testIntersects(
        "POLYGON((0 0, 0 15, 15 15, 15 0, 0 0))",
        "POLYGON((15 15, 15 20, 20 20, 20 15, 15 15))"
    ));
    assert(!testIntersects(
        "POLYGON((0 0, 0 14, 14 14, 14 0, 0 0))",
        "POLYGON((15 15, 15 20, 20 20, 20 15, 15 15))"
    ));
    return 1;
}

int test_GeomPolyMapWithin(){
    geom g1 = decode("POLYGON((1 1, 1 9, 9 9, 9 1, 1 1))");
    geom g2 = decode("POLYGON((5 5, 5 6, 6 6, 6 5, 5 5))");
    geom g3 = decode("POLYGON((15 5, 15 6, 16 6, 16 5, 15 5))");
    assert(g1 && g2 && g3);
    geomPolyMap *m1 = geomNewPolyMap(g1);
    geomPolyMap *m2 = geomNewPolyMap(g2);
    geomPolyMap *m3 = geomNewPolyMap(g3);
    assert(m1 && m2 && m3);

    assert(geomPolyMapWithin(m2, m1));
    assert(!geomPolyMapWithin(m3, m1));


    geomFreePolyMap(m1);
    geomFreePolyMap(m2);
    geomFreePolyMap(m3);
    geomFree(g1);
    geomFree(g2);
    geomFree(g3);
    return 1;
}






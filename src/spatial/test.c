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

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "test.h"

int test_Geom();
int test_GeomZ();
int test_GeomZM();
int test_GeomPoint();
int test_GeomMultiPoint();
int test_GeomLineString();
int test_GeomMultiLineString();
int test_GeomPolygon();
int test_GeomMultiPolygon();
int test_GeomGeometryCollection();
int test_GeomIterator();
int test_GeomPolyMap();
int test_RTreeInsert();
int test_RTreeSearch();
int test_RTreeRemove();
int test_GeoUtilDistance();
int test_GeoUtilDestination();
int test_PolyRayInside();
int test_PolyRayExteriorHoles();
int test_PolyInsideShapes();
int test_PolyIntersectsLines();
int test_PolyIntersectsShapes();
int test_PolyRectIntersects();
int test_PolyRectInside();


int test_GeomPolyMapPointBench();
int test_GeomPolyMapPolygonBench();
int test_GeomPolyMapGeometryCollectionBench();

int test_GeomPolyMapPointBenchSingleThreaded();
int test_GeomPolyMapPolygonBenchSingleThreaded();
int test_GeomPolyMapGeometryCollectionBenchSingleThreaded();

int test_GeomPolyMapIntersects();
int test_GeomPolyMapWithin();




typedef struct test{
	char *name;
	int (*test)();
} test;

test tests[] = {
	{ "geom", test_Geom },
	{ "geomZ", test_GeomZ },
	{ "geomZM", test_GeomZM },
	{ "geomPoint", test_GeomPoint },
	{ "geomMultiPoint", test_GeomMultiPoint },
	{ "geomLineString", test_GeomLineString },
	{ "geomMultiLineString", test_GeomMultiLineString },
	{ "geomPolygon", test_GeomPolygon },
	{ "geomMultiPolygon", test_GeomMultiPolygon },
	{ "geomGeometryCollection", test_GeomGeometryCollection },
	{ "geomIterator", test_GeomIterator },
	{ "geomPolyMap", test_GeomPolyMap },
	
	{ "rtreeInsert", test_RTreeInsert },
	{ "rtreeSearch", test_RTreeSearch },
	{ "rtreeRemove", test_RTreeRemove },

	{ "geoutilDistance", test_GeoUtilDistance },
	{ "geoutilDestination", test_GeoUtilDestination },

	{ "polyRayInside", test_PolyRayInside },
	{ "polyRayExteriorHoles", test_PolyRayExteriorHoles },
	{ "polyInsideShapes", test_PolyInsideShapes },
	{ "polyIntersectsLines", test_PolyIntersectsLines },
	{ "polyIntersectsShapes", test_PolyIntersectsShapes },
	{ "polyRectIntersects", test_PolyRectIntersects },
	{ "polyRectInside", test_PolyRectInside },

	{ "polyMapPointBench", test_GeomPolyMapPointBench },
	{ "polyMapPolygonBench", test_GeomPolyMapPolygonBench },
	{ "polyMapGeomColBench", test_GeomPolyMapGeometryCollectionBench },
	{ "polyMapPointBenchSingleThreaded", test_GeomPolyMapPointBenchSingleThreaded },
	{ "polyMapPolygonBenchSingleThreaded", test_GeomPolyMapPolygonBenchSingleThreaded },
	{ "polyMapGeomColBenchSingleThreaded", test_GeomPolyMapGeometryCollectionBenchSingleThreaded },

	{ "searchPolyMapIntersects", test_GeomPolyMapIntersects },
	{ "searchPolyMapWithin", test_GeomPolyMapWithin },

};

static int abort_handled = 0;
void __tassert_fail(const char *what, const char *file, int line, const char *func){
	printf("\x1b[31m[failed]\x1b[0m\n");
	printf("    fail: assert(%s)\n    func: %s\n    file: %s\n    line: %d\n", what, func, file, line);
	abort_handled = 1;
	abort();
}

void sig_handler(int sig) {
	if (!abort_handled){
		printf("\x1b[31m[failed]\x1b[0m\n");
		if (sig == SIGABRT){
			printf("    \x1b[33muse \"test.h\" for more details.\x1b[0m\n");
		}
		abort_handled = 1;
	}else {
		printf("\x1b[0m\n");
	}
	if (sig == SIGSEGV){
		printf("Segmentation fault: 11\n");
	}
	exit(1);
}

static clock_t start_c = 0;
static clock_t stop_c = 0;

void stopClock(){
	stop_c = clock();
}

void restartClock(){
	start_c = clock();
	stop_c = 0;
}

int main(int argc, const char **argv) {
	signal(SIGSEGV, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGABRT, sig_handler);

	const char *run = getenv("RUNTEST");
	if (run == NULL){
		run = "";
	}

	for (int i=1;i<argc;i++){
		const char *arg = argv[i];
		if (strcmp(arg, "-r") == 0 || strcmp(arg, "-run") == 0 || strcmp(arg, "--run") == 0){
			if (i+1==argc){
				fprintf(stderr, "argument '%s' requires a value\n", arg);
				exit(-1);	
			}
			arg = argv[i+1];
			i++;
			run = arg;
		} else {
			fprintf(stderr, "unknown argument '%s'\n", arg);
			exit(-1);
		}
	}

	int mlabelsz = 0;
	for (int i=0;i<sizeof(tests)/sizeof(test);i++){
		test t = tests[i];
		if (!(strlen(run) == 0 || strstr(t.name, run) != 0)){
			continue;
		}
		char label[50];
		sprintf(label, "  testing %s", t.name); 
		if (strlen(label) > mlabelsz){
			mlabelsz = strlen(label);
		}
	}

	for (int i=0;i<sizeof(tests)/sizeof(test);i++){
		test t = tests[i];
		if (!(strlen(run) == 0 || strstr(t.name, run) != 0)){
			continue;
		}
		char label[50];
		sprintf(label, "  testing %s", t.name); 
		char lszs[10];
		sprintf(lszs, "%%-%ds", mlabelsz+3);
		fprintf(stdout, lszs, label);
		fflush(stdout);
		restartClock();
		int res = t.test();
		if (stop_c == 0){
			stopClock();
		}
		double elapsed = ((double)(stop_c-start_c)/(double)CLOCKS_PER_SEC);
		if (!res){
			printf("\x1b[31m[failed]\x1b[0m\n");
		} else{
			printf("\x1b[32m[ok]\x1b[0m");
			printf(" %5.2f secs", elapsed);
			if (res > 1){
				double opss = ((double)res)/elapsed;
				printf(", op/s %.0f", opss);
			}
			printf("\n");
		}
	}
}

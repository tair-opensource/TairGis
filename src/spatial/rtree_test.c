#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "test.h"
#include "rtree.h"

static double randd() { return ((rand()%RAND_MAX) / (double)RAND_MAX); }
static double randx() { return randd() * 360.0 - 180.0; }
static double randy() { return randd() * 180.0 - 90.0; }

int iterator(double minX, double minY, double maxX, double maxY, void *item, void *userdata){
	return 1;
}

static rtree *insert(){
	srand(time(NULL)/clock());
	rtree *tr = rtreeNew();
	assert(tr);

	int n = 100000;
	//clock_t start = clock();
	for (int i=0;i<n;i++){
		double minX = randx();
		double minY = randy();
		double maxX = minX+(randd()*10+0.0001);
		double maxY = minY+(randd()*10+0.0001);
		assert(rtreeInsert(tr, minX, minY, maxX, maxY, (void*)(long)i));
	}
	//double elapsed = (double)(clock()-start)/(double)CLOCKS_PER_SEC;
	//printf("inserted %d items in %.2f secs, %.0f ops/s\n", n, elapsed, (double)n/elapsed);
	assert(rtreeCount(tr)==n);
	return tr;
}

int test_RTreeInsert(){
	//printf("rtree implementation\n");
	srand(time(NULL)/clock());
	rtree *tr = insert();
	rtreeFree(tr);
	return 1;
}

int test_RTreeSearch(){
	srand(time(NULL)/clock());
	rtree *tr = insert();

	double rtC = 0;
	int n = 10000;
	//clock_t start = clock();
	for (int i=0;i<n;i++){
		rtC += rtreeSearch(tr, 0, 0, 1, 1, iterator, NULL);
	}
	//double elapsed = ((double)(clock()-start) / (double)CLOCKS_PER_SEC);
	assert(rtC/n >= 40 && rtC/n <= 80);
	//printf("searched %d queries in %.2f secs, %.0f ops/s (%.0f items)\n", n, elapsed, (double)n/elapsed, rtC/(double)n);
	rtreeFree(tr);
	return 1;
}


int test_RTreeRemove(){
	rtree *tr = rtreeNew();
	assert(tr);

	assert(rtreeInsert(tr, 10, 10, 20, 20, (void*)(long)100));
	assert(rtreeInsert(tr, 30, 30, 50, 50, (void*)(long)101));
	assert(rtreeCount(tr)==2);

	assert(rtreeRemove(tr, 30, 30, 50, 50, (void*)(long)101));	
	assert(rtreeCount(tr)==1);
	assert(!rtreeRemove(tr, 30, 30, 50, 50, (void*)(long)101));	

	assert(rtreeRemove(tr, 0, 0, 30, 30, (void*)(long)100));	
	assert(rtreeCount(tr)==0);
	assert(!rtreeRemove(tr, 0, 0, 30, 30, (void*)(long)100));	

	rtreeRemoveAll(tr);
	assert(rtreeCount(tr)==0);

	assert(rtreeInsert(tr, 10, 10, 20, 20, (void*)(long)100));
	assert(rtreeInsert(tr, 30, 30, 50, 50, (void*)(long)101));
	assert(rtreeCount(tr)==2);

	rtreeRemoveAll(tr);
	assert(rtreeCount(tr)==0);
	assert(!rtreeRemove(tr, 10, 10, 20, 20, (void*)(long)100));
	assert(!rtreeRemove(tr, 30, 30, 50, 50, (void*)(long)101));



	rtreeFree(tr);
	return 1;
}
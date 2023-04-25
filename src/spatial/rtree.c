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

#define NUM_DIMS 2

#include "zmalloc.h"
#include "rtree_tmpl.c"
#include "rtree.h"

typedef struct rtreeIterator {
	iteratorT *iterator;
} rtreeIterator;

rtree *rtreeNew() {
	rtree *tr = zmalloc(sizeof(rtree));
	if (!tr){
		return NULL;
	}
	memset(tr, 0, sizeof(rtree));
	return tr;
}

void rtreeFree(rtree *tr){
	if (!tr){
		return;
	}
	if (tr->root){
		freeNode(tr->root);
	}
	zfree(tr);
}

/* Remove removes item from rtree */
int rtreeRemove(rtree *tr, double minX, double minY, double maxX, double maxY, void *item) {
	if (tr && tr->root) {
		return removeRect(makeRect(minX, minY, maxX, maxY), item, &(tr->root)) ? 0 : 1;
	}
	return 0;
}

// Count return the number of items in rtree.
int rtreeCount(rtree *tr) {
	if (!tr || !tr->root){
		return 0;
	}
	return countRec(tr->root, 0);
}

// Insert inserts item into rtree
int rtreeInsert(rtree *tr, double minX, double minY, double maxX, double maxY, void *item) {
	if (!tr){
		return 0;
	}
	if (!tr->root) {
		tr->root = zmalloc(sizeof(nodeT));
		if (!tr->root){
			return 0;
		}
		memset(tr->root, 0, sizeof(nodeT));
	}
	insertRect(makeRect(minX, minY, maxX, maxY), item, NULL, &(tr->root), 0);
	return 1;
}

void rtreeRemoveAll(rtree *tr){
	if (tr && tr->root){
		freeNode(tr->root);
		tr->root = NULL;
	}
}

typedef struct iteratorUserData {
	rtreeSearchFunc iterator;
	void *userdata;
} iteratorUserData;

static int iteratorFunc(rectT rect, void *item, void *userdata){
	iteratorUserData *ud = userdata;
	double minX, minY, maxX, maxY;
	getRect(rect, &minX, &minY, &maxX, &maxY);
	return ud->iterator(minX, minY, maxX, maxY, item, ud->userdata);
}

int rtreeSearch(rtree *tr, double minX, double minY, double maxX, double maxY, rtreeSearchFunc iterator, void *userdata){
	if (!tr || !tr->root){
		return 0;
	}
	if (iterator) {
		iteratorUserData ud = {iterator, userdata};
		return search(tr->root, makeRect(minX, minY, maxX, maxY), iteratorFunc, &ud);
	} else{
		return search(tr->root, makeRect(minX, minY, maxX, maxY), NULL, NULL);
	}
}

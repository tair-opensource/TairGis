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

/* An r-tree implementation.
 *
 * This file is derived from the original work done by Toni Gutman. R-Trees: 
 * A Dynamic Index Structure for Spatial Searching, Proc. 1984 ACM SIGMOD 
 * International Conference on Management of Data, pp. 47-57. 
 * 
 * This source file is based on the C++ code which can be found at 
 * "http://www.superliminal.com/sources/sources.htm".
 *
 * The C++ source license is "Entirely free for all uses. Enjoy!"
 *
 */

/* Template options */
#ifndef NUMBER
#   define NUMBER double
#endif
#ifndef NUM_DIMS
#   define NUM_DIMS 2
#endif
#ifndef USE_SPHERICAL_VOLUME
#   define USE_SPHERICAL_VOLUME 1
#endif
#ifndef MAX_NODES
#   define MAX_NODES 16
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "zmalloc.h"

#define MIN_NODES (MAX_NODES/2)

#if NUM_DIMS == 2
#   define UNIT_SPHERE_VOLUME 3.141593
#elif NUM_DIMS == 3
#   define UNIT_SPHERE_VOLUME 4.188790
#elif NUM_DIMS == 4
#   define UNIT_SPHERE_VOLUME 4.934802
#else
#   error invalid NUM_DIMS only 2,3,4 allowed
#endif

typedef struct branchT branchT;
typedef struct nodeT nodeT;
typedef struct rectT rectT;
typedef struct listNodeT listNodeT;
typedef struct partitionVarsT partitionVarsT;
typedef struct stackT stackT;
typedef struct iteratorT iteratorT;

struct rectT {
    NUMBER min[NUM_DIMS];
    NUMBER max[NUM_DIMS];
};

struct branchT {
    rectT rect;
    void  *item;
    nodeT *child;
};

struct nodeT {
    int     count;
    int     level;
    branchT branch[MAX_NODES];
};

struct listNodeT {
    listNodeT *next;
    nodeT     *node;
};

struct partitionVarsT {
    int     partition[MAX_NODES+1];
    int     total;
    int     minFill;
    int     taken[MAX_NODES+1];
    int     count[2];
    rectT   cover[2];
    NUMBER  area[2];
    branchT branchBuf[MAX_NODES+1];
    int     branchCount;
    rectT   coverSplit;
    NUMBER  coverSplitArea;
};

static int addBranch(branchT *branch, nodeT *node, nodeT **newNode);
static int pickBranch(rectT rect, nodeT *node);

#if NUM_DIMS == 2
static inline rectT makeRect(NUMBER minX, NUMBER minY, NUMBER maxX, NUMBER maxY) {
    rectT rect;
    rect.min[0] = minX;
    rect.min[1] = minY;
    rect.max[0] = maxX;
    rect.max[1] = maxY;
    return rect;
}
static inline void getRect(rectT rect, NUMBER *minX, NUMBER *minY, NUMBER *maxX, NUMBER *maxY) {
    *minX = rect.min[0];
    *minY = rect.min[1];
    *maxX = rect.max[0];
    *maxY = rect.max[1];
}
#elif NUM_DIMS == 3
static inline rectT makeRect(NUMBER minX, NUMBER minY, NUMBER minZ, NUMBER maxX, NUMBER maxY, NUMBER maxZ) {
    rectT rect;
    rect.min[0] = minX;
    rect.min[1] = minY;
    rect.min[2] = minZ;
    rect.max[0] = maxX;
    rect.max[1] = maxY;
    rect.max[2] = maxZ;
    return rect;
}
static inline void getRect(rectT rect, NUMBER *minX, NUMBER *minY, NUMBER *minZ, NUMBER *maxX, NUMBER *maxY, NUMBER *maxZ) {
    *minX = rect.min[0];
    *minY = rect.min[1];
    *minZ = rect.min[2];
    *maxX = rect.max[0];
    *maxY = rect.max[1];
    *maxZ = rect.max[2];
}
#elif NUM_DIMS == 4
static inline rectT makeRect(NUMBER minX, NUMBER minY, NUMBER minZ, NUMBER minM, NUMBER maxX, NUMBER maxY, NUMBER maxZ, NUMBER maxM) {
    rectT rect;
    rect.min[0] = minX;
    rect.min[1] = minY;
    rect.min[2] = minZ;
    rect.min[3] = minM;
    rect.max[0] = maxX;
    rect.max[1] = maxY;
    rect.max[2] = maxZ;
    rect.max[3] = maxM;
    return rect;
}
static inline void getRect(rectT rect, NUMBER *minX, NUMBER *minY, NUMBER *minZ, NUMBER *minM, NUMBER *maxX, NUMBER *maxY, NUMBER *maxZ, NUMBER *maxM) {
    *minX = rect.min[0];
    *minY = rect.min[1];
    *minZ = rect.min[2];
    *minM = rect.min[3];
    *maxX = rect.max[0];
    *maxY = rect.max[1];
    *maxZ = rect.max[2];
    *maxM = rect.max[3];
}
#endif

static inline NUMBER min(NUMBER a, NUMBER b) {
    if (a < b) {
        return a;
    }
    return b;
}

static inline NUMBER max(NUMBER a, NUMBER b) {
    if (a > b) {
        return a;
    }
    return b;
}

static inline int overlap(rectT rectA, rectT rectB) {
    for (int index = 0; index < NUM_DIMS; index++) {
        if (rectA.min[index] > rectB.max[index] ||
            rectB.min[index] > rectA.max[index]) {
            return 0;
        }
    }
    return 1;
}

/* push node to listNode for further reinsert */
static void reinsert(nodeT *node, listNodeT **listNode) {
    listNodeT *nlistNode = zmalloc(sizeof(listNodeT));
    memset(nlistNode, 0, sizeof(listNodeT));
    nlistNode->node = node;
    nlistNode->next = *listNode;
    *listNode = nlistNode;
}

/* move the last of branch here, and decrease the count of node */
static void disconnectBranch(nodeT *node, int index) {
    node->branch[index] = node->branch[node->count-1];
    node->count--;
}

static rectT combineRect(rectT rectA, rectT rectB) {
    rectT newRect;
    memset(&newRect, 0, sizeof(rectT));
    for (int index = 0; index < 2; index++) {
        newRect.min[index] = min(rectA.min[index], rectB.min[index]);
        newRect.max[index] = max(rectA.max[index], rectB.max[index]);
    }
    return newRect;
}

/* recalculate the rectangle of node */
static rectT nodeCover(nodeT *node) {
    int firstTime = 1;
    rectT rect;
    memset(&rect, 0, sizeof(rectT));
    for (int index = 0; index < node->count; index++) {
        if (firstTime) {
            rect = node->branch[index].rect;
            firstTime = 0;
        } else {
            rect = combineRect(rect, node->branch[index].rect);
        }
    }
    return rect;
}

static NUMBER rectVolume(rectT rect) {
    NUMBER volume = 1;
    for (int index = 0; index < 2; index++) {
        volume *= rect.max[index] - rect.min[index];
    }
    return volume;
}

static NUMBER rectSphericalVolume(rectT rect) {
    NUMBER sumOfSquares = 0;
    NUMBER radius = 0;
    for (int index = 0; index < 2; index++) {
        NUMBER halfExtent = (rect.max[index] - rect.min[index]) * 0.5;
        sumOfSquares += halfExtent * halfExtent;
    }
    radius = sqrt(sumOfSquares);
    if (NUM_DIMS==2){
        return radius*radius*UNIT_SPHERE_VOLUME;
    } else if (NUM_DIMS==3){
        return radius*radius*radius*UNIT_SPHERE_VOLUME;
    } else if (NUM_DIMS==4){
        return radius*radius*radius*radius*UNIT_SPHERE_VOLUME;
    } else{
        return pow(radius, NUM_DIMS) * UNIT_SPHERE_VOLUME;
    }
}

static NUMBER calcRectVolume(rectT rect) {
    if (USE_SPHERICAL_VOLUME) {
        return rectSphericalVolume(rect);
    }
    return rectVolume(rect);
}

static void getBranches(nodeT *node, branchT *branch, partitionVarsT *parVars) {
    for (int index = 0; index < MAX_NODES; index++) {
        parVars->branchBuf[index] = node->branch[index];
    }
    parVars->branchBuf[MAX_NODES] = *branch;
    parVars->branchCount = MAX_NODES + 1;
    parVars->coverSplit = parVars->branchBuf[0].rect;
    for (int index = 1; index < MAX_NODES+1; index++) {
        parVars->coverSplit = combineRect(parVars->coverSplit, parVars->branchBuf[index].rect);
    }
    parVars->coverSplitArea = calcRectVolume(parVars->coverSplit);
    node->count = 0;
    node->level = -1;
}

static void initParVars(partitionVarsT *parVars, int maxRects, int minFill) {
    parVars->count[1] = 0;
    parVars->count[0] = parVars->count[1];
    parVars->area[1] = 0;
    parVars->area[0] = parVars->area[1];
    parVars->total = maxRects;
    parVars->minFill = minFill;
    for (int index = 0; index < maxRects; index++) {
        parVars->taken[index] = 0;
        parVars->partition[index] = -1;
    }
}

static void classify(int index, int group, partitionVarsT *parVars) {
    parVars->partition[index] = group;
    parVars->taken[index] = 1;

    if (parVars->count[group] == 0) {
        parVars->cover[group] = parVars->branchBuf[index].rect;
    } else {
        parVars->cover[group] = combineRect(parVars->branchBuf[index].rect, parVars->cover[group]);
    }
    parVars->area[group] = calcRectVolume(parVars->cover[group]);
    parVars->count[group]++;
}

static void pickSeeds(partitionVarsT *parVars) {
    int seed0 = 0;
    int seed1 = 0;
    NUMBER worst = 0;
    NUMBER waste = 0;
    NUMBER area[MAX_NODES+1];
    memset(&area, 0, sizeof(area));
    for (int index = 0; index < parVars->total; index++) {
        area[index] = calcRectVolume(parVars->branchBuf[index].rect);
    }
    worst = -parVars->coverSplitArea - 1;
    for (int indexA = 0; indexA < parVars->total-1; indexA++) {
        for (int indexB = indexA + 1; indexB < parVars->total; indexB++) {
            rectT oneRect = combineRect(parVars->branchBuf[indexA].rect, parVars->branchBuf[indexB].rect);
            waste = calcRectVolume(oneRect) - area[indexA] - area[indexB];
            if (waste > worst) {
                worst = waste;
                seed0 = indexA;
                seed1 = indexB;
            }
        }
    }
    classify(seed0, 0, parVars);
    classify(seed1, 1, parVars);
}

static void choosePartition(partitionVarsT *parVars, int minFill) {
    NUMBER biggestDiff = 0;
    int group = 0;
    int chosen = 0;
    int betterGroup = 0;
    initParVars(parVars, parVars->branchCount, minFill);
    pickSeeds(parVars);
    while (((parVars->count[0] + parVars->count[1]) < parVars->total) &&
        (parVars->count[0] < (parVars->total - parVars->minFill)) &&
        (parVars->count[1] < (parVars->total - parVars->minFill))) {
        biggestDiff = -1;
        for (int index = 0; index < parVars->total; index++) {
            if (!parVars->taken[index]) {
                rectT curRect = parVars->branchBuf[index].rect;
                rectT rect0 = combineRect(curRect, parVars->cover[0]);
                rectT rect1 = combineRect(curRect, parVars->cover[1]);
                NUMBER growth0 = calcRectVolume(rect0) - parVars->area[0];
                NUMBER growth1 = calcRectVolume(rect1) - parVars->area[1];
                NUMBER diff = growth1 - growth0;
                if (diff >= 0) {
                    group = 0;
                } else {
                    group = 1;
                    diff = -diff;
                }
                if (diff > biggestDiff) {
                    biggestDiff = diff;
                    chosen = index;
                    betterGroup = group;
                } else if ((diff == biggestDiff) && (parVars->count[group] < parVars->count[betterGroup])) {
                    chosen = index;
                    betterGroup = group;
                }
            }
        }
        classify(chosen, betterGroup, parVars);
    }
    if ((parVars->count[0] + parVars->count[1]) < parVars->total) {
        if (parVars->count[0] >= parVars->total-parVars->minFill) {
            group = 1;
        } else {
            group = 0;
        }
        for (int index = 0; index < parVars->total; index++) {
            if (!parVars->taken[index]) {
                classify(index, group, parVars);
            }
        }
    }
}

static void loadNodes(nodeT *nodeA, nodeT *nodeB, partitionVarsT *parVars) {
    for (int index = 0; index < parVars->total; index++) {
        if (parVars->partition[index] == 0) {
            addBranch(&parVars->branchBuf[index], nodeA, NULL);
        } else if (parVars->partition[index] == 1) {
            addBranch(&parVars->branchBuf[index], nodeB, NULL);
        }
    }
}

static void splitNode(nodeT *node, branchT *branch, nodeT **newNode) {
    partitionVarsT localVars;
    memset(&localVars, 0, sizeof(partitionVarsT));
    partitionVarsT *parVars = &localVars;
    int level = 0;
    level = node->level;
    getBranches(node, branch, parVars);
    choosePartition(parVars, MIN_NODES);
    *newNode = zmalloc(sizeof(nodeT));
    memset(*newNode, 0, sizeof(nodeT));
    node->level = level;
    (*newNode)->level = node->level;
    loadNodes(node, *newNode, parVars);
}

static int addBranch(branchT *branch, nodeT *node, nodeT **newNode) {
    if (node->count < MAX_NODES) {
        node->branch[node->count] = *branch;
        node->count++;
        return 0;
    }
    splitNode(node, branch, newNode);
    return 1;
}

static void freeNode(nodeT *node){
    if (!node) {
        return;
    }
    for (int i=0;i<node->count;i++) {
        if (node->branch[i].child) {
            freeNode(node->branch[i].child);
        }
    }
    zfree(node);
}

static int insertRectRec(rectT rect, void *item, nodeT *child, nodeT *node, nodeT **newNode, int level) {
    int index = 0;
    branchT branch;
    memset(&branch, 0, sizeof(branchT));
    nodeT *otherNode = NULL;
    if (node == NULL) {
        return 0;
    }
    if (node->level > level) {
        index = pickBranch(rect, node);
        if (!insertRectRec(rect, item, child, node->branch[index].child, &otherNode, level)) {
            node->branch[index].rect = combineRect(rect, node->branch[index].rect);
            return 0;
        }
        node->branch[index].rect = nodeCover(node->branch[index].child);
        branch.child = otherNode;
        branch.rect = nodeCover(otherNode);
        return addBranch(&branch, node, newNode);
    } else if (node->level == level) {
        branch.rect = rect;
        branch.item = item;
        branch.child = child;
        return addBranch(&branch, node, newNode);
    }
    return 0;
}

static int insertRect(rectT rect, void *item, nodeT *child, void **vroot, int level) {
    nodeT **root = (nodeT **) vroot;
    nodeT *newRoot = NULL;
    nodeT *newNode = NULL;
    branchT branch;
    memset(&branch, 0, sizeof(branchT));
    if (insertRectRec(rect, item, child, *root, &newNode, level)) {
        newRoot = zmalloc(sizeof(nodeT));
        memset(newRoot, 0, sizeof(nodeT));
        newRoot->level = (*root)->level + 1;
        branch.rect = nodeCover(*root);
        branch.child = *root;
        addBranch(&branch, newRoot, NULL);
        branch.rect = nodeCover(newNode);
        branch.child = newNode;
        addBranch(&branch, newRoot, NULL);
        *root = newRoot;
        return 1;
    }
    return 0;
}

static int pickBranch(rectT rect, nodeT *node) {
    int firstTime = 1;
    NUMBER increase = 0;
    NUMBER bestIncr = -1;
    NUMBER area = 0;
    NUMBER bestArea = 0;
    int best = 0;
    rectT tempRect;
    memset(&tempRect, 0, sizeof(rectT));
    for (int index = 0; index < node->count; index++) {
        rectT curRect = node->branch[index].rect;
        area = calcRectVolume(curRect);
        tempRect = combineRect(rect, curRect);
        increase = calcRectVolume(tempRect) - area;
        if ((increase < bestIncr) || firstTime) {
            best = index;
            bestArea = area;
            bestIncr = increase;
            firstTime = 0;
        } else if ((increase == bestIncr) && (area < bestArea)) {
            best = index;
            bestArea = area;
            bestIncr = increase;
        }
    }
    return best;
}

static int countRec(nodeT *node, int counter) {
    if (node->level > 0) {
        for (int index = 0; index < node->count; index++) {
            counter = countRec(node->branch[index].child, counter);
        }
    } else {
        counter += node->count;
    }
    return counter;
}

static int removeRectRec(rectT rect, void *item, nodeT *node, listNodeT **listNode) {
    if (node == NULL) {
        return 1;
    }
    if (node->level > 0) {
        for (int index = 0; index < node->count; index++) {
            if (overlap(rect, node->branch[index].rect)) {
                if (!removeRectRec(rect, item, node->branch[index].child, listNode)) {
                    if (node->branch[index].child->count >= MIN_NODES) {
                        node->branch[index].rect = nodeCover(node->branch[index].child);
                    } else {
                        reinsert(node->branch[index].child, listNode);
                        disconnectBranch(node, index);
                    }
                    return 0;
                }
            }
        }
    } else {
        for (int index = 0; index < node->count; index++) {
            if (node->branch[index].item == item) {
                disconnectBranch(node, index);
                return 0;
            }
        }
    }
    return 1;
}

/* rectangle remove is resource cost operation
 * try to skip this operation as we can
 * return 0 if actually deleted, otherwise return 1 */
static int removeRect(rectT rect, void *item, void **vroot) {
    nodeT **root = (nodeT **) vroot;
    nodeT *tempNode = NULL;
    listNodeT *reinsertList = NULL;
    if (!removeRectRec(rect, item, *root, &reinsertList)) {
        while (reinsertList != NULL) {
            tempNode = reinsertList->node;
            for (int index = 0; index < tempNode->count; index++) {
                insertRect(tempNode->branch[index].rect,
                    tempNode->branch[index].item,
                    tempNode->branch[index].child,
                    vroot,
                    tempNode->level);
            }
            listNodeT *prev = reinsertList;
            reinsertList = reinsertList->next;
            if (prev->node) zfree(prev->node);
            zfree(prev);
        }
        if ((*root)->count == 1 && (*root)->level > 0) {
            tempNode = (*root)->branch[0].child;
            *root = tempNode;
        }
        return 0;
    }
    return 1;
}

static int search(nodeT *node, rectT rect, int(*iterator)(rectT rect, void *item, void *userdata), void *userdata){
    int counter = 0;
    if (node) {
        if (node->level > 0) {
            for (int index = 0; index < node->count; index++) {
                if (overlap(rect, node->branch[index].rect)) {
                    counter += search(node->branch[index].child, rect, iterator, userdata);
                }
            }
        } else {
            for (int index = 0; index < node->count; index++) {
                if (overlap(rect, node->branch[index].rect)) {
                    if (iterator){
                        if (!iterator(node->branch[index].rect, node->branch[index].item, userdata)){
                            return counter;
                        }
                    }
                    counter++;
                }
            }
        }
    }
    return counter;
}



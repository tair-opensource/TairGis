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

#ifndef BING_H_
#define BING_H_

#if defined(__cplusplus)
extern "C" {
#endif

int bingMapSize(int levelOfDetail);
void bingLatLongToPixelXY(double latitude, double longitude, int levelOfDetail, int *pixelX, int *pixelY);
void bingPixelXYToLatLong(int pixelX, int pixelY, int levelOfDetail, double *latitude, double *longitude);
void bingPixelXYToTileXY(int pixelX, int pixelY, int *tileX, int *tileY);
void bingTileXYToPixelXY(int tileX, int tileY, int *pixelX, int *pixelY);
void bingTileXYToQuadKey(int tileX, int tileY, int levelOfDetail, char *quadKey);
int bingQuadKeyToTileXY(char *quadKey, int *tileX, int *tileY, int *levelOfDetail);
void bingTileXYToBounds(int tileX, int tileY, int levelOfDetail, double *minLat, double *minLon, double *maxLat, double *maxLon);
int bingQuadKeyToBounds(char *quadkey, double *minLat, double *minLon, double *maxLat, double *maxLon);
void bingLatLongToQuadKey(double latitude, double longitude, int levelOfDetail, char *quadkey);
void bingLatLonToTileXY(double latitude, double longitude, int levelOfDetail, int *tileX, int *tileY);


#if defined(__cplusplus)
}
#endif
#endif /* BING_H_ */

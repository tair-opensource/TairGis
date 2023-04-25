// https://msdn.microsoft.com/en-us/library/bb259689.aspx

#include <math.h>
#include <string.h>
#include "bing.h"

#ifndef PI
#define PI 3.14159265358979323846
#endif


//static const double EarthRadius = 6378137.0;
static const double MinLatitude = -85.05112878;
static const double MaxLatitude = 85.05112878;
static const double MinLongitude = -180.0;
static const double MaxLongitude = 180.0;
static const int TileSize = 256;
//static const int MaxLevelOfDetail = 38;


static double clip(double n, double minValue, double maxValue) {
	if (n < minValue) {
		return minValue;
	}
	if (n > maxValue) {
		return maxValue;
	}
	return n;
}

int bingMapSize(int levelOfDetail) {
	return TileSize << levelOfDetail;
}

void bingLatLongToPixelXY(double latitude, double longitude, int levelOfDetail, int *pixelX, int *pixelY) {
	latitude = clip(latitude, MinLatitude, MaxLatitude);
	longitude = clip(longitude, MinLongitude, MaxLongitude);
	double x = (longitude + 180) / 360;
	double sinLatitude = sin(latitude * PI / 180);
	double y = 0.5 - log((1+sinLatitude)/(1-sinLatitude))/(4*PI);
	double mapSize = bingMapSize(levelOfDetail);
	if (pixelX) *pixelX = clip(x*mapSize+0.5, 0, mapSize-1);
	if (pixelY) *pixelY = clip(y*mapSize+0.5, 0, mapSize-1);
}

void bingPixelXYToLatLong(int pixelX, int pixelY, int levelOfDetail, double *latitude, double *longitude) {
	double mapSize = bingMapSize(levelOfDetail);
	double x = (clip(pixelX, 0, mapSize-1) / mapSize) - 0.5;
	double y = 0.5 - (clip(pixelY, 0, mapSize-1) / mapSize);
	if (latitude) *latitude = 90 - 360*atan(exp(-y*2*PI))/PI;
	if (longitude) *longitude = 360 * x;
}

void bingPixelXYToTileXY(int pixelX, int pixelY, int *tileX, int *tileY) {
	if (tileX) *tileX = pixelX >> 8;
	if (tileY) *tileY = pixelY >> 8;
}

void bingTileXYToPixelXY(int tileX, int tileY, int *pixelX, int *pixelY) {
	if (pixelX) *pixelX = tileX << 8;
	if (pixelY) *pixelY = tileY << 8;
}

void bingTileXYToQuadKey(int tileX, int tileY, int levelOfDetail, char *quadKey) {
	for (int i=levelOfDetail,j=0;i>0;i--,j++) {
		int mask = 1 << (i - 1);
		if ((tileX & mask) != 0) {
			if ((tileY & mask) != 0) {
				quadKey[j] = '3';
			} else {
				quadKey[j] = '1';
			}
		} else if ((tileY & mask) != 0) {
			quadKey[j] = '2';
		} else {
			quadKey[j] = '0';
		}
	}
}

int bingQuadKeyToTileXY(char *quadKey, int *tileX, int *tileY, int *levelOfDetail) {
	int ltileX = 0;
	int ltileY = 0;
	int llevelOfDetail = strlen(quadKey);
	for (int i=llevelOfDetail;i>0;i--) {
		int mask = 1 << (i - 1);
		switch (quadKey[llevelOfDetail-i]) {
		case '0':
			break;
		case '1':
			ltileX |= mask;
			break;
		case '2':
			ltileY |= mask;
			break;
		case '3':
			ltileX |= mask;
			ltileY |= mask;
			break;
		default:
			return 0;
		}
	}
	if (tileX) *tileX = ltileX;
	if (tileY) *tileY = ltileY;
	if (levelOfDetail) *levelOfDetail = llevelOfDetail;

	return 1;
}

void bingTileXYToBounds(int tileX, int tileY, int levelOfDetail, double *minLat, double *minLon, double *maxLat, double *maxLon) {
	int size = 1 << levelOfDetail;
	int pixelX, pixelY;
	bingTileXYToPixelXY(tileX, tileY, &pixelX, &pixelY);
	bingPixelXYToLatLong(pixelX, pixelY, levelOfDetail, maxLat, minLon);
	bingTileXYToPixelXY(tileX+1, tileY+1, &pixelX, &pixelY);
	bingPixelXYToLatLong(pixelX, pixelY, levelOfDetail, minLat, maxLon);
	if (tileX%size == 0) {
		if (minLon) *minLon = MinLongitude;
	}
	if (tileX%size == size-1) {
		if (maxLon) *maxLon = MaxLongitude;
	}
	if (tileY <= 0) {
		if (maxLat) *maxLat = MaxLatitude;
	}
	if (tileY >= size-1) {
		if (minLat) *minLat = MinLatitude;
	}
}

int bingQuadKeyToBounds(char *quadkey, double *minLat, double *minLon, double *maxLat, double *maxLon){
	int tileX, tileY, levelOfDetail;
	if (!bingQuadKeyToTileXY(quadkey, &tileX, &tileY, &levelOfDetail)){
		return 0;
	}
	bingTileXYToBounds(tileX, tileY, levelOfDetail, minLat, minLon, maxLat, maxLon);
	return 1;
}

void bingLatLonToTileXY(double latitude, double longitude, int levelOfDetail, int *tileX, int *tileY){
	int pixelX, pixelY;
	bingLatLongToPixelXY(latitude, longitude, levelOfDetail, &pixelX, &pixelY);
	bingPixelXYToTileXY(pixelX, pixelY, tileX, tileY);
}

void bingLatLongToQuadKey(double latitude, double longitude, int levelOfDetail, char *quadkey){
	int tileX, tileY;
	bingLatLonToTileXY(latitude, longitude, levelOfDetail, &tileX, &tileY);
	bingTileXYToQuadKey(tileX, tileY, levelOfDetail, quadkey);
}







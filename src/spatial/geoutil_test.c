#include <math.h>
#include <stdio.h>
#include "test.h"
#include "geoutil.h"

static double tdist(double latA, double latB, double lonA, double lonB){
	return floor(geoutilDistance(latA, latB, lonA, lonB)*100)/100;
}

int test_GeoUtilDistance(){
	assert(tdist(33, -115, 33.5, -115.5)==72476.64);
	return 1;
}

int test_GeoUtilDestination(){
	double lat, lon;
	geoutilDestinationLatLon(33, -115, 100000, 90, &lat, &lon);
	assert(fabs(lat - 32.995417)<0.00001 && fabs(lon - -113.927719)<0.00001);
	return 1;
}

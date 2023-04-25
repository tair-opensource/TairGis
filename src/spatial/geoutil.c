/* Origin */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
/* Latitude/longitude spherical geodesy tools                         (c) Chris Veness 2002-2016  */
/*                                                                                   MIT Licence  */
/* www.movable-type.co.uk/scripts/latlong.html                                                    */
/* www.movable-type.co.uk/scripts/geodesy/docs/module-latlon-spherical.html                       */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */

/* Modified and new stuff */
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

#include <math.h>
#include <string.h>
#include "geoutil.h"

#ifndef PI
#define PI 3.14159265358979323846
#endif

#define EARTH_RADIUS 6372797.560856
#define RAD(deg) ((deg)*PI/180.0)
#define DEG(rad) ((rad)*180.0/PI)

double geoutilDistance(double latA, double lonA, double latB, double lonB){
	double q1 = RAD(latA);
	double a1 = RAD(lonA);
	double q2 = RAD(latB);
	double a2 = RAD(lonB);
	double aq = q2 - q1;
	double av = a2 - a1;
	double a = sin(aq/2)*sin(aq/2) + cos(q1)*cos(q2)*sin(av/2)*sin(av/2);
	double c = 2 * atan2(sqrt(a), sqrt(1-a));
	return EARTH_RADIUS * c;
}

void geoutilDestinationLatLon(double lat, double lon, double distanceMeters, double bearingDegrees, double *destLat, double *destLon){
	if (!destLat || !destLon){
		return;
	}
	double tq = distanceMeters / EARTH_RADIUS; // angular distance in radians
	double Z = RAD(bearingDegrees);
	double q1 = RAD(lat);
	double a1 = RAD(lon);
	double q2 = asin(sin(q1)*cos(tq) + cos(q1)*sin(tq)*cos(Z));
	double a2 = a1 + atan2(sin(Z)*sin(tq)*cos(q1), cos(tq)-sin(q1)*sin(q2));
	a2 = fmod(a2+3*PI, 2*PI) - PI; // normalise to -180..+180°
	*destLat = DEG(q2);
	*destLon = DEG(a2);
}

geomRect geoutilBoundsFromLatLon(double centerLat, double centerLon, double distanceMeters){
	geomRect r;
	memset(&r, 0, sizeof(geomRect));
	double nLat, nLon, eLat, eLon, sLat, sLon, wLat, wLon;
	geoutilDestinationLatLon(centerLat, centerLon, distanceMeters, 0, &nLat, &nLon);
	geoutilDestinationLatLon(centerLat, centerLon, distanceMeters, 90, &eLat, &eLon);
	geoutilDestinationLatLon(centerLat, centerLon, distanceMeters, 180, &sLat, &sLon);
	geoutilDestinationLatLon(centerLat, centerLon, distanceMeters, 270, &wLat, &wLon);
	r.min.x = wLon;
	r.min.y = sLat;
	r.max.x = eLon;
	r.max.y = nLat;
	return r;
}



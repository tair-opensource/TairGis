/* Geohash implementation ported from Javascript to C */

/* Javascript copyright
 *
 * Geohash encoding/decoding and associated functions
 * (c) Chris Veness 2014 / MIT Licence  
 */

/* C copyright
 *
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

#include "hash.h"

static inline int base32R(char b);
static inline char base32F(int i);

int hashEncode(double lat, double lon, int precision, char *hash){
	int idx = 0; // index into base32 map
	int bit = 0; // each char holds 5 bits
	int evenBit = 1;
	int glen = 0;
	double latMin = -90.0;
	double latMax = 90.0;
	double lonMin = -180.0;
	double lonMax = 180.0;
	if (precision < 1 || !hash) {
		return 0;
	}
	
	while (glen < precision) {
		if (evenBit) {
			// bisect E-W longitude
			double lonMid = (lonMin + lonMax) / 2;
			if (lon > lonMid) {
				idx = idx*2 + 1;
				lonMin = lonMid;
			} else {
				idx = idx * 2;
				lonMax = lonMid;
			}
		} else {
			// bisect N-S latitude
			double latMid = (latMin + latMax) / 2;
			if (lat > latMid) {
				idx = idx*2 + 1;
				latMin = latMid;
			} else {
				idx = idx * 2;
				latMax = latMid;
			}
		}
		evenBit = evenBit?0:1;

		bit = bit + 1;
		if (bit == 5) {
			// 5 bits gives us a character: append it and start over
			char b = base32F(idx);
			if (!b) {
				return 0;
			}
			hash[glen++] = b;
			bit = 0;
			idx = 0;
		}
	}
	hash[glen] = '\0';
	return glen;
}

int hashBounds(char *hash, double *swLat, double *swLon, double *neLat, double *neLon){
	int evenBit = 1;
	double latMin = -90.0;
	double latMax = 90.0;
	double lonMin = -180.0;
	double lonMax = 180.0;
	char *p = hash;
	while (*p) {
		char chr = *p;
		int idx = base32R(chr);
		if (idx == -1) {
			return 0;
		}
		for (int n=4;n>=0;n--) {
			int bitN = idx >> n & 1;
			if (evenBit) {
				// longitude
				double lonMid = (lonMin + lonMax) / 2;
				if (bitN == 1) {
					lonMin = lonMid;
				} else {
					lonMax = lonMid;
				}
			} else {
				// latitude
				double latMid = (latMin + latMax) / 2;
				if (bitN == 1) {
					latMin = latMid;
				} else {
					latMax = latMid;
				}
			}
			evenBit = evenBit?0:1;
			if (n == 0) {
				break;
			}
		}
		p++;
	}
	if (swLat) *swLat = latMin;
	if (swLon) *swLon = lonMin;
	if (neLat) *neLat = latMax;
	if (neLon) *neLon = lonMax;
	return 1;
}

int hashDecode(char *hash, double *lat, double *lon) {
	if (!hash){
		return 0;
	}
	double swLat, swLon, neLat, neLon;
	if (!hashBounds(hash, &swLat, &swLon, &neLat, &neLon)){
		return 0;
	}
	if (lat) *lat = (neLat-swLat)/2 + swLat;
	if (lon) *lon = (neLon-swLon)/2 + swLon;
	return 1;
}

static inline int base32R(char b) {
	switch (b) {
	default:return -1;
	case '0':return 0;
	case '1':return 1;
	case '2':return 2;
	case '3':return 3;
	case '4':return 4;
	case '5':return 5;
	case '6':return 6;
	case '7':return 7;
	case '8':return 8;
	case '9':return 9;
	case 'b':case 'B':return 10;
	case 'c':case 'C':return 11;
	case 'd':case 'D':return 12;
	case 'e':case 'E':return 13;
	case 'f':case 'F':return 14;
	case 'g':case 'G':return 15;
	case 'h':case 'H':return 16;
	case 'j':case 'J':return 17;
	case 'k':case 'K':return 18;
	case 'm':case 'M':return 19;
	case 'n':case 'N':return 20;
	case 'p':case 'P':return 21;
	case 'q':case 'Q':return 22;
	case 'r':case 'R':return 23;
	case 's':case 'S':return 24;
	case 't':case 'T':return 25;
	case 'u':case 'U':return 26;
	case 'v':case 'V':return 27;
	case 'w':case 'W':return 28;
	case 'x':case 'X':return 29;
	case 'y':case 'Y':return 30;
	case 'z':case 'Z':return 31;
	}
}

static char base32[32] = "0123456789bcdefghjkmnpqrstuvwxyz";

static inline char base32F(int i) {
	if (i<0||i>31){
		return 0;
	}
	return base32[i];
}



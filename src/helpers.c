/*
	File: helpers.c
	Author: Philip Haynes
*/

#include "helpers.h"

float trif(float x) {
	x /= 3.1415926535;
	while (x < 0)
		x += 4;
	while (x > 4)
		x -= 4;
	if (x > 3)
		return 4.0 - x;
	if (x < 1)
		return -x;
	return x - 2.0;
}

float sqrf(float x) {
	x /= 3.1415926535;
	while (x < 0)
		x += 2;
	while (x > 2)
		x -= 2;
	if (x > 1)
		return 1;
	return -1;
}

float sinc(float x) {
	if (x == 0)
		return 1.0f;
	float temp = x * 3.1415926535;
	return sinf(temp) / x;
}

float cosc(float x) {
	if (x < -1.0 || x > 1.0)
		return 0.0;
	return cosf(x * 3.1415926535) * 0.5 + 0.5;
}

float linc(float x) {
	if (x > 1.0f || x < -1.0f)
	return 0.0f;
	if (x > 0)
		return 1.0f - x;
	else
		return 1.0f + x;
}

float cubic(float a, float b, float c, float d, float x) {
	return b + 0.5 * x * (c - a + x * (2 * a - 5 * b + 4 * c - d + x * (3 * (b - c) + d - a)));
}
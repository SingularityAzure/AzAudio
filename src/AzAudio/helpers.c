/*
	File: helpers.c
	Author: Philip Haynes
*/

#include "helpers.h"

#include <assert.h>

float trif(float x) {
	x /= AZA_PI;
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
	x /= AZA_PI;
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
	float temp = x * AZA_PI;
	return sinf(temp) / x;
}

float cosc(float x) {
	if (x < -1.0 || x > 1.0)
		return 0.0;
	return cosf(x * AZA_PI) * 0.5 + 0.5;
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

size_t aza_grow(size_t startSize, size_t minSize, size_t alignment) {
	assert(alignment > 0);
	startSize = AZA_MAX(startSize, alignment);
	while (startSize < minSize) {
		startSize = aza_align(startSize * 3 / 2, alignment);
	}
	return startSize;
}

float clampf(float a, float minimum, float maximum) {
	return a < minimum ? minimum : (a > maximum ? maximum : a);
}

float aza_db_to_ampf(float db) {
	return powf(10.0f, db/20.0f);
}

float aza_amp_to_dbf(float amp) {
	if (amp < 0.0f) amp = 0.0f;
	return log10f(amp)*20.0f;
}

size_t aza_ms_to_samples(float ms, float samplerate) {
	return ms * samplerate * 0.001f;
}

size_t aza_align(size_t size, size_t alignment) {
	return (size + alignment-1) & ~(alignment-1);
}

size_t aza_align_non_power_of_two(size_t size, size_t alignment) {
	if (size % alignment == 0) {
		return size;
	} else {
		return (size/alignment+1)*alignment;
	}
}
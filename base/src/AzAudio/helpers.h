/*
	File: helpers.h
	Author: Philip Haynes
	Just some utility functions. Not to be included in headers.
*/

#ifndef AZAUDIO_HELPERS_H
#define AZAUDIO_HELPERS_H

#include "AzAudio.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZA_LOG_ERR(...) azaLog(AZA_LOG_LEVEL_ERROR, __VA_ARGS__)
#define AZA_LOG_INFO(...) azaLog(AZA_LOG_LEVEL_INFO, __VA_ARGS__)
#define AZA_LOG_TRACE(...) azaLog(AZA_LOG_LEVEL_TRACE, __VA_ARGS__)

float trif(float x);

float sqrf(float x);

float sinc(float x);

float cosc(float x);

float linc(float x);

// sinc with a hann window with a total size of 1+2*radius
float lanczos(float x, float radius);

float clampf(float a, float min, float max);

static inline float lerp(float a, float b, float t) {
	return a + (b - a) * t;
}

static inline float linstepf(float a, float min, float max) {
	return clampf((a - min) / (max - min), 0.0f, 1.0f);
}

// Like a % max except the answer is always in the range [0; max) even if the input is negative
static inline int wrapi(int a, int max) {
	// assert(max > 0);
	if (a < 0) {
		return (a + 1) % max + max-1;
	} else if (a > 0) {
		return a % max;
	} else {
		return 0;
	}
}

float cubic(float a, float b, float c, float d, float x);

float aza_db_to_ampf(float db);

float aza_amp_to_dbf(float amp);

size_t aza_ms_to_samples(float ms, float samplerate);

size_t aza_align(size_t size, size_t alignment);

size_t aza_align_non_power_of_two(size_t size, size_t alignment);

// Grows the size by 3/2 repeatedly until it's at least as big as minSize
size_t aza_grow(size_t size, size_t minSize, size_t alignment);

#define AZA_MAX(a, b) ((a) > (b) ? (a) : (b))

#define AZA_MIN(a, b) ((a) < (b) ? (a) : (b))

#define AZA_CLAMP(a, min, max) AZA_MAX(min, AZA_MIN(max, a))

#define AZA_SAMPLES_TO_MS(samples, samplerate) ((float)(samples) / (float)(samplerate) * 1000.0f)

// Returns the 32-bit signed integer representation of a 24-bit integer stored in the lower 24 bits of a u32. You don't have to worry about what's in the high 8 bits as they'll be masked out.
int32_t signExtend24Bit(uint32_t value);

#ifndef AZA_LIKELY
#ifdef __GNUC__
#define AZA_LIKELY(x) (__builtin_expect(!!(x),1))
#define AZA_UNLIKELY(x) (__builtin_expect(!!(x),0))
#else
#define AZA_LIKELY(x) (x)
#define AZA_UNLIKELY(x) (x)
#endif
#endif

void azaStrToLower(char *dst, size_t dstSize, const char *src);

#ifdef __cplusplus
}
#endif

#endif // AZAUDIO_HELPERS_H
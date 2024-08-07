/*
	File: helpers.h
	Author: Philip Haynes
	Just some utility functions. Not to be included in headers.
*/

#ifndef AZAUDIO_HELPERS_H
#define AZAUDIO_HELPERS_H

#include <math.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

float trif(float x);

float sqrf(float x);

float sinc(float x);

float cosc(float x);

float linc(float x);

float cubic(float a, float b, float c, float d, float x);

float clampf(float a, float minimum, float maximum);

float aza_db_to_ampf(float db);

float aza_amp_to_dbf(float amp);

size_t aza_ms_to_samples(float ms, float samplerate);

size_t aza_align(size_t size, size_t alignment);

size_t aza_align_non_power_of_two(size_t size, size_t alignment);

// Grows the size by 3/2 repeatedly until it's at least as big as minSize
size_t aza_grow(size_t size, size_t minSize, size_t alignment);

#define AZA_MAX(a, b) ((a) > (b) ? (a) : (b))

#define AZA_SAMPLES_TO_MS(samples, samplerate) ((float)(samples) / (float)(samplerate) * 1000.0f)

#ifdef __cplusplus
}
#endif

#endif // AZAUDIO_HELPERS_H
/*
	File: helpers.h
	Author: Philip Haynes
	Just some utility functions. Not to be used in headers.
*/

#ifndef AZAUDIO_HELPERS_H
#define AZAUDIO_HELPERS_H

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

float trif(float x);

float sqrf(float x);

float sinc(float x);

float cosc(float x);

float linc(float x);

float cubic(float a, float b, float c, float d, float x);

#ifdef __cplusplus
}
#endif

#endif // AZAUDIO_HELPERS_H
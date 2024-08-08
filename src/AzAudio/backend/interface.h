/*
	File: interface.h
	Author: Philip Haynes
	Defining the unified interface for backends with function pointers to be assigned based on available backends.
*/

#ifndef AZAUDIO_INTERFACE_H
#define AZAUDIO_INTERFACE_H

#include "../dsp.h"

#ifdef __cplusplus
extern "C" {
#endif

// Find out what backends are available, picks one, and set up function pointers.
int azaBackendInit();
void azaBackendDeinit();

typedef enum azaDeviceInterface {
	AZA_OUTPUT,
	AZA_INPUT,
} azaDeviceInterface;

typedef int (*fp_azaMixCallback)(azaBuffer buffer, void *userData);

typedef struct {
	// backend-specific data
	void *data;
	
	// User configuration
	
	azaDeviceInterface deviceInterface;
	// Leave at 0 for device default
	size_t samplerate;
	// Leave at 0 for device default
	size_t channels;
	fp_azaMixCallback mixCallback;
	void *userdata;
} azaStream;

typedef int (*fp_azaStreamInit)(azaStream *stream, const char *device);
extern fp_azaStreamInit azaStreamInit;

typedef void (*fp_azaStreamDeinit)(azaStream *stream);
extern fp_azaStreamDeinit azaStreamDeinit;

#ifdef __cplusplus
}
#endif

#endif // AZAUDIO_INTERFACE_H
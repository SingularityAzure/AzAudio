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

typedef struct azaStream {
	// backend-specific data
	void *data;

	// User configuration

	// leave as NULL for default device
	const char *deviceName;
	azaDeviceInterface deviceInterface;
	// Leave at 0 for device default
	uint32_t samplerate;
	// Leave at 0 for device default
	// formFactor is ignored
	azaChannelLayout channels;
	fp_azaMixCallback mixCallback;
	void *userdata;
} azaStream;

typedef int (*fp_azaStreamInit)(azaStream *stream);
extern fp_azaStreamInit azaStreamInit;

typedef void (*fp_azaStreamDeinit)(azaStream *stream);
extern fp_azaStreamDeinit azaStreamDeinit;

typedef void (*fp_azaStreamSetActive)(azaStream *stream, uint8_t active);
extern fp_azaStreamSetActive azaStreamSetActive;

typedef uint8_t (*fp_azaStreamGetActive)(azaStream *stream);
extern fp_azaStreamGetActive azaStreamGetActive;

typedef const char* (*fp_azaStreamGetDeviceName)(azaStream *stream);
extern fp_azaStreamGetDeviceName azaStreamGetDeviceName;

typedef uint32_t (*fp_azaStreamGetSamplerate)(azaStream *stream);
extern fp_azaStreamGetSamplerate azaStreamGetSamplerate;

typedef azaChannelLayout (*fp_azaStreamGetChannelLayout)(azaStream *stream);
extern fp_azaStreamGetChannelLayout azaStreamGetChannelLayout;

typedef size_t (*fp_azaGetDeviceCount)(azaDeviceInterface interface);
extern fp_azaGetDeviceCount azaGetDeviceCount;

typedef const char* (*fp_azaGetDeviceName)(azaDeviceInterface interface, size_t index);
extern fp_azaGetDeviceName azaGetDeviceName;

typedef size_t (*fp_azaGetDeviceChannels)(azaDeviceInterface interface, size_t index);
extern fp_azaGetDeviceChannels azaGetDeviceChannels;

#ifdef __cplusplus
}
#endif

#endif // AZAUDIO_INTERFACE_H
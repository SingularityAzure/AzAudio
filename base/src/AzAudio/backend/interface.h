/*
	File: interface.h
	Author: Philip Haynes
	Defining the unified interface for backends with function pointers to be assigned based on available backends.
*/

#ifndef AZAUDIO_INTERFACE_H
#define AZAUDIO_INTERFACE_H

#include "../dsp.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Find out what backends are available, picks one, and set up function pointers.
int azaBackendInit();
void azaBackendDeinit();

typedef enum azaDeviceInterface {
	AZA_OUTPUT=0,
	AZA_INPUT,
} azaDeviceInterface;

typedef struct azaStreamConfig {
	// A device's name as returned from azaGetDeviceName. Must be an exact match, else falls back to the default.
	// leave as NULL for default device
	// NOTE: The user is responsible for this string's lifetime (we don't make an internal copy).
	const char *deviceName;
	// Leave at 0 for device default
	uint32_t samplerate;
	// Leave at 0 for device default
	// formFactor is ignored
	azaChannelLayout channelLayout;
} azaStreamConfig;

typedef struct azaStream {
	azaStreamConfig config;
	// Are we an AZA_INPUT or AZA_OUTPUT device? A zero value indicates AZA_OUTPUT.
	azaDeviceInterface deviceInterface;
	// This will be called whenever new samples are needed or produced by the backend.
	fp_azaMixCallback mixCallback;
	// Passed into mixCallback
	void *userdata;
	// backend-specific data
	void *data;
} azaStream;

enum azaStreamFlags {
	// Sets azaStreamConfig.deviceName to the default device's name (included for completeness, but you probably don't want to do this)
	// Does nothing if you specify a device name.
	AZA_STREAM_COMMIT_DEVICE_NAME    = 0x01,
	// Sets azaStreamConfig samplerate to the device's default samplerate.
	// You may want to use this if you have processing that requires the samplerate to not change.
	// Does nothing if you specify a samplerate.
	AZA_STREAM_COMMIT_SAMPLERATE     = 0x02,
	// Sets azaStreamConfig channelLayout to the device's default channelLayout.
	// You may want to use this if you have processing that requires the channelLayout to not change.
	// Does nothing if you specify a channel layout.
	AZA_STREAM_COMMIT_CHANNEL_LAYOUT = 0x04,
	// Sets azaStreamConfig samplerate and channelLayout to the device's default values.
	// Does nothing if you specify a samplerate and channelLayout
	AZA_STREAM_COMMIT_FORMAT = AZA_STREAM_COMMIT_SAMPLERATE | AZA_STREAM_COMMIT_CHANNEL_LAYOUT,
	// Sets azaStreamConfig fields to the default device's default values (included for completeness, but you probably just want AZA_STREAM_COMMIT_FORMAT)
	AZA_STREAM_COMMIT_ALL = AZA_STREAM_COMMIT_DEVICE_NAME | AZA_STREAM_COMMIT_FORMAT,
};

// config is used to select from available devices and configure the stream on that device. If the device doesn't support the exact format desired, AzAudio will convert between the nearest natively-available format and your desired format.
// deviceInterface can be either AZA_OUTPUT or AZA_INPUT
// flags is a combination of azaStreamFlags
// if activate is true then the stream will immediately activate without having to call azaStreamSetActive
typedef int (*fp_azaStreamInit)(azaStream *stream, azaStreamConfig config, azaDeviceInterface deviceInterface, uint32_t flags, bool activate);
extern fp_azaStreamInit azaStreamInit;

// Helper that chooses defaults for everything.
static inline int azaStreamInitDefault(azaStream *stream, azaDeviceInterface deviceInterface, bool activate) {
	return azaStreamInit(stream, AZA_CLITERAL(azaStreamConfig) {0}, deviceInterface, 0, activate);
}

typedef void (*fp_azaStreamDeinit)(azaStream *stream);
extern fp_azaStreamDeinit azaStreamDeinit;

typedef void (*fp_azaStreamSetActive)(azaStream *stream, bool active);
extern fp_azaStreamSetActive azaStreamSetActive;

typedef bool (*fp_azaStreamGetActive)(azaStream *stream);
extern fp_azaStreamGetActive azaStreamGetActive;

typedef const char* (*fp_azaStreamGetDeviceName)(azaStream *stream);
extern fp_azaStreamGetDeviceName azaStreamGetDeviceName;

typedef uint32_t (*fp_azaStreamGetSamplerate)(azaStream *stream);
extern fp_azaStreamGetSamplerate azaStreamGetSamplerate;

typedef azaChannelLayout (*fp_azaStreamGetChannelLayout)(azaStream *stream);
extern fp_azaStreamGetChannelLayout azaStreamGetChannelLayout;

typedef uint32_t (*fp_azaStreamGetBufferFrameCount)(azaStream *stream);
extern fp_azaStreamGetBufferFrameCount azaStreamGetBufferFrameCount;

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
/*
	File: audio.h
	Author: singularity
	C bindings for core functions

	Most functions will return an int to indicate error status, where 0 is success
*/

//#define SYS_AUDIO_NO_STDIO // to remove stdio dependency

#ifndef AZAUDIO_H
#define AZAUDIO_H

#include "dsp.h"

#ifdef __cplusplus
extern "C" {
#endif

// Error handling

// The operation completed successfully
#define AZA_SUCCESS 0
// A pointer was unexpectedly null
#define AZA_ERROR_NULL_POINTER 1
// A multi-channel function was passed zero or fewer channels
#define AZA_ERROR_INVALID_CHANNEL_COUNT 2
// A multi-frame function was passed zero or fewer frames
#define AZA_ERROR_INVALID_FRAME_COUNT 3

#define AZA_CHANNELS 2
#define AZA_SAMPLERATE 48000

// Latency should be equal to AZAUDIO_PLAYBACK_BUFFERS * AZAUDIO_FRAMES_PER_BUFFER samples

// What we use to buffer sound data for the callback function to consume or write to
//extern float *inputBuffer, *outputBuffer;
//extern int inputBufferSize, outputBufferSize;
//extern int inputLocation, outputLocation; // Location in ring buffers

// Setup / Errors

int azaInit();

int azaClean();

int azaGetError();

void azaDefaultLogFunc(const char* message);

// We use a callback function for all message logging.
// This allows the user to define their own logging output functions
typedef void (*azafpLogCallback)(const char* message);

int azaSetLogCallback(azafpLogCallback newLogFunc);

extern azafpLogCallback azaPrint;

// Allows custom mixing functions
// NOTE: User must provide data structs for every effect used
typedef int (*azafpMixCallback)(const float *input, float *output, unsigned long frames, int channels, void *userData);

//  Data structures

typedef struct {
	void *data;
	int capture; // Are we input or output?
	unsigned sampleRate;
	azafpMixCallback mixCallback;
} azaStream;

typedef struct {
	azaBuffer buffer;
	azaSamplerData *samplerData;
	azaHighPassData *highPassData;
	azaReverbData *reverbData;
	azaCompressorData *compressorData;
	azaLookaheadLimiterData *limiterData;
	// Static parameters
	int channels;
} azaDefaultMixData;
int azaDefaultMixDataInit(azaDefaultMixData *data);
int azaDefaultMixDataClean(azaDefaultMixData *data);

// Core functionality

int azaInitStream(azaStream *stream, const char *device, int capture, azafpMixCallback mixCallback);
void azaDeinitStream(azaStream *stream);

#ifdef __cplusplus
}
#endif

#endif // AZAUDIO_H

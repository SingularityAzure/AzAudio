/*
    File: audio.h
    Author: singularity
    C bindings for core functions

    Most functions will return an int to indicate error status, where 0 is success

    This library uses PortAudio for cross-platform device interface.
        PortAudio Portable Real-Time Audio Library
        Copyright (c) 1999-2011 Ross Bencina, Phil Burk, MIT License
*/

//#define SYS_AUDIO_NO_STDIO // to remove stdio dependency

#ifndef AZURE_AUDIO_H
#define AZURE_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

// Error handling
#define AZA_SUCCESS 0
    // The operation completed successfully
#define AZA_ERROR_NULL_POINTER 1
    // A pointer was unexpectedly null
#define AZA_ERROR_PORTAUDIO 2
    // PortAudio has generated an error
#define AZA_ERROR_INVALID_CHANNEL_COUNT 3
    // A multi-channel function was passed zero or fewer channels
#define AZA_ERROR_INVALID_FRAME_COUNT 4
    // A multi-frame function was passed zero or fewer frames

#define AZURE_AUDIO_RMS_SAMPLES 128
#define AZURE_AUDIO_LOOKAHEAD_SAMPLES 128
#define AZURE_AUDIO_SAMPLER_TRANSITION_FRAMES 128
    // The duration of transitions between the variable parameter values
#define AZURE_AUDIO_FRAMES_PER_BUFFER 64

// Setup / Errors

int azaInit();

int azaClean();

int azaGetError();

void azaDefaultLogFunc(const char* message);

// We use a callback function for all message logging.
// This allows the user to define their own logging output functions
typedef void (*fpLogCallback)(const char* message);

int azaSetLogCallback(fpLogCallback newLogFunc);

extern fpLogCallback azaPrint;

int azaDefaultMixFunc(const float *input, float *output, unsigned long frames, int channels, void *userData);

// Allows custom mixing functions
// NOTE: User must provide data structs for every effect used
typedef int (*fpMixCallback)(const float *input, float *output, unsigned long frames, int channels, void *userData);

int azaSetMixCallback(fpMixCallback newMixFunc);

extern fpMixCallback azaMix;

//  Data structures

typedef struct {
    float *samples;
    // Static paramaters
    int frames;
} azaBuffer;
int azaBufferInit(azaBuffer *data);
int azaBufferClean(azaBuffer *data);

typedef struct {
    float squared;
    float buffer[AZURE_AUDIO_RMS_SAMPLES];
    int index;
} azaRmsData;
void azaRmsDataInit(azaRmsData *data);

typedef struct {
    float output;
    // Static parameters
    float samplerate;
    float frequency;
} azaFilterData;
typedef azaFilterData azaLowPassData;
typedef azaFilterData azaHighPassData;
void azaLowPassDataInit(azaLowPassData *data);
void azaHighPassDataInit(azaHighPassData *data);

typedef struct {
    float gainBuffer[AZURE_AUDIO_LOOKAHEAD_SAMPLES];
    float valBuffer[AZURE_AUDIO_LOOKAHEAD_SAMPLES];
    int index;
    float sum;
    // Static parameters
    float gain;
} azaLookaheadLimiterData;
void azaLookaheadLimiterDataInit(azaLookaheadLimiterData *data);

typedef struct {
    azaRmsData rms;
    float attenuation;
    float gain; // For monitoring/debugging
    // Static parameters
    float samplerate;
    float threshold;
    float ratio;
    float attack;
    float decay;
} azaCompressorData;
void azaCompressorDataInit(azaCompressorData *data);

typedef struct {
    float *buffer; // Must be dynamically-allocated to allow different time spans
    int index;
    // Static parameters
    float feedback;
    float amount;
    int samples;
} azaDelayData;
void azaDelayDataInit(azaDelayData *data);
void azaDelayDataClean(azaDelayData *data);

#define AZURE_AUDIO_REVERB_DELAY_COUNT 15

typedef struct {
    azaDelayData delay[AZURE_AUDIO_REVERB_DELAY_COUNT];
    azaLowPassData lowPass[AZURE_AUDIO_REVERB_DELAY_COUNT];
    // Static parameters
    float amount;
    float roomsize;
    float color;
    int samplesOffset;
} azaReverbData;
void azaReverbDataInit(azaReverbData *data);
void azaReverbDataClean(azaReverbData *data);

typedef struct {
    float frame;
    float s; // Smooth speed
    float g; // Smooth gain
    // Static parameters
    azaBuffer *buffer;
    // Variable parameters
    float speed;
    float gain;
} azaSamplerData;
int azaSamplerDataInit(azaSamplerData *data);

typedef struct {
    void *stream;
} azaStream;

typedef struct {
    azaBuffer buffer;
    azaSamplerData *samplerData;
    azaHighPassData *highPassData;
    azaCompressorData *compressorData;
    azaLookaheadLimiterData *limiterData;
    // Static parameters
    int channels;
} azaDefaultMixData;
int azaDefaultMixDataInit(azaDefaultMixData *data);
int azaDefaultMixDataClean(azaDefaultMixData *data);

// Core functionality

int azaMicTestStart(azaStream *stream);

int azaMicTestStop(azaStream *stream);

// Returns the root mean square (RMS) loudness
int azaRms(const float *input, float *output, azaRmsData *data, int frames, int channels);

// Simple distortion to smooth harsh peaking
int azaCubicLimiter(const float *input, float *output, int frames, int channels);

/*  gain is in db
    NOTE: This limiter increases latency by AZURE_AUDIO_LOOKAHEAD_SAMPLES samples     */
int azaLookaheadLimiter(const float *input, float *output, azaLookaheadLimiterData *data, int frames, int channels);

/*  threshold is in db
    ratio is defined as 1/x for positive values
        becomes absolute for negative values (where -1 is the same as infinity)
    attack and decay are in milliseconds        */
int azaCompressor(const float *input, float *output, azaCompressorData *data, int frames, int channels);

int azaDelay(const float *input, float *output, azaDelayData *data, int frames, int channels);

int azaReverb(const float *input, float *output, azaReverbData *data, int frames, int channels);

int azaLowPass(const float *input, float *output, azaLowPassData *data, int frames, int channels);

int azaHighPass(const float *input, float *output, azaHighPassData *data, int frames, int channels);

int azaSampler(const float *input, float *output, azaSamplerData *data, int frames, int channels);

#ifdef __cplusplus
}
#endif

#endif // AZURE_AUDIO_H

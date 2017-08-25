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

#define AZURE_AUDIO_RMS_SAMPLES 128
#define AZURE_AUDIO_LOOKAHEAD_SAMPLES 128

int azaInit();

int azaGetError();

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
    int samples;
    // Static parameters
    float feedback;
    float amount;
} azaDelayData;
void azaDelayDataInit(azaDelayData *data, int samples); // length in samples of the buffer
void azaDelayDataClean(azaDelayData *data);

#define AZURE_AUDIO_REVERB_DELAY_COUNT 15

typedef struct {
    azaDelayData delay[AZURE_AUDIO_REVERB_DELAY_COUNT];
    azaLowPassData lowPass[AZURE_AUDIO_REVERB_DELAY_COUNT];
    // Static parameters
    float amount;
    float roomsize;
    float color;
} azaReverbData;
void azaReverbDataInit(azaReverbData *data, int samples[AZURE_AUDIO_REVERB_DELAY_COUNT]);
void azaReverbDataClean(azaReverbData *data);

void azaDefaultLogFunc(const char* message);

// We use a callback function for all message logging.
// This allows the user to define their own logging output functions
typedef void (*fpLogCallback)(const char* message);

// void newLogFunc(const char* message);
int azaSetLogCallback(fpLogCallback newLogFunc);

extern fpLogCallback azaPrint;

typedef struct {
    void *stream;
} azaStream;

typedef struct {
    azaLookaheadLimiterData limiterData[2];
    azaCompressorData compressorData[2];
    azaDelayData delayData[2];
    azaReverbData reverbData[2];
    azaHighPassData highPassData[2];
} azaMixData;
void azaMixDataInit(azaMixData *data);
void azaMixDataClean(azaMixData *data);

int azaMicTestStart(azaStream *stream, azaMixData *data);

int azaMicTestStop(azaStream *stream, azaMixData *data);

// Returns the root mean square (RMS) loudness
float azaRms(float input, azaRmsData *data);

// Simple distortion to smooth harsh peaking
float azaCubicLimiter(float input);

/*  gain is in db
    NOTE: This limiter increases latency by AZURE_AUDIO_LOOKAHEAD_SAMPLES samples     */
float azaLookaheadLimiter(float input, azaLookaheadLimiterData *data);

/*  threshold is in db
    ratio is defined as 1/x for positive values
        becomes absolute for negative values (where -1 is the same as infinity)
    attack and decay are in milliseconds        */
float azaCompressor(float input, azaCompressorData *data);

float azaDelay(float input, azaDelayData *data);

float azaReverb(float input, azaReverbData *data);

float azaLowPass(float input, azaLowPassData *data);

float azaHighPass(float input, azaHighPassData *data);

#ifdef __cplusplus
}
#endif

#endif // AZURE_AUDIO_H

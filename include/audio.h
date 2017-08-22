/*
    File: audio.h
    Author: singularity
    C bindings for core functions

    Most functions will return an int to indicate error status, where 0 is success
*/

//#define SYS_AUDIO_NO_STDIO // to remove stdio dependency

#ifndef SYS_AUDIO_H
#define SYS_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

// Error handling
#define AZA_SUCCESS 0
#define AZA_ERROR_NULL_POINTER 1
#define AZA_ERROR_PORTAUDIO 2

#define AZA_RMS_SAMPLES 128
#define AZA_LOOKAHEAD_SAMPLES 128

void azaInit();

typedef struct {
    float squared;
    float buffer[AZA_RMS_SAMPLES];
    int index;
} azaRmsData;
void azaRmsDataInit(azaRmsData *data);

typedef struct {
    float output;
} azaFilterData;
typedef azaFilterData azaLowPassData;
typedef azaFilterData azaHighPassData;
void azaLowPassDataInit(azaLowPassData *data);
void azaHighPassDataInit(azaHighPassData *data);

typedef struct {
    float gainBuffer[AZA_LOOKAHEAD_SAMPLES];
    float valBuffer[AZA_LOOKAHEAD_SAMPLES];
    int index;
    float gain;
} azaLookaheadLimiterData;
void azaLookaheadLimiterDataInit(azaLookaheadLimiterData *data);

typedef struct {
    azaRmsData rms;
    float attenuation;
    float gain; // For monitoring
} azaCompressorData;
void azaCompressorDataInit(azaCompressorData *data);

typedef struct {
    float *buffer; // Must be dynamically-allocated to allow different time spans
    int index;
    int samples;
} azaDelayData;
void azaDelayDataInit(azaDelayData *data, int samples); // length in samples of the buffer
void azaDelayDataClean(azaDelayData *data);

#define AZA_REVERB_DELAY_COUNT 15

typedef struct {
    azaDelayData delay[AZA_REVERB_DELAY_COUNT];
    azaLowPassData lowPass[AZA_REVERB_DELAY_COUNT];
} azaReverbData;
void azaReverbDataInit(azaReverbData *data, int samples[AZA_REVERB_DELAY_COUNT]);
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
    azaLowPassData lowPassData[2];
} azaMixData;
void azaMixDataInit(azaMixData *data);
void azaMixDataClean(azaMixData *data);

int azaMicTestStart(azaStream *stream, azaMixData *data);

int azaMicTestStop(azaStream *stream, azaMixData *data);

/*  gain is in db
    This limiter increases latency by AZA_LOOKAHEAD_SAMPLES samples     */
float azaLookaheadLimiter(float input, azaLookaheadLimiterData *data, float gain);

/*  threshold is in db
    ratio is defined as 1/x for positive values
        becomes absolute for negative values (where -1 is the same as infinity)
    attack and decay are in milliseconds        */
float azaCompressor(float input, azaCompressorData *data, float samplerate,
            float threshold, float ratio, float attack, float decay);

float azaDelay(float input, azaDelayData *data, float feedback, float amount);

float azaReverb(float input, azaReverbData *data, float amount);

float azaLowPass(float input, azaLowPassData *data, float samplerate, float frequency);

float azaHighPass(float input, azaLowPassData *data, float samplerate, float frequency);

#ifdef __cplusplus
}
#endif

#endif // SYS_AUDIO_H

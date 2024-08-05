/*
	File: dps.h
	Author: Philip Haynes
	structs and functions for digital signal processing
*/

#ifndef AZAUDIO_DSP_H
#define AZAUDIO_DSP_H

#ifdef __cplusplus
extern "C" {
#endif

#define AZAUDIO_RMS_SAMPLES 128
#define AZAUDIO_LOOKAHEAD_SAMPLES 128
// The duration of transitions between the variable parameter values
#define AZAUDIO_SAMPLER_TRANSITION_FRAMES 128

typedef struct {
	float *samples;
	// Static paramaters
	int frames;
} azaBuffer;
int azaBufferInit(azaBuffer *data);
int azaBufferClean(azaBuffer *data);

typedef struct {
	float squared;
	float buffer[AZAUDIO_RMS_SAMPLES];
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
	float gainBuffer[AZAUDIO_LOOKAHEAD_SAMPLES];
	float valBuffer[AZAUDIO_LOOKAHEAD_SAMPLES];
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

#define AZAUDIO_REVERB_DELAY_COUNT 15

typedef struct {
	azaDelayData delay[AZAUDIO_REVERB_DELAY_COUNT];
	azaLowPassData lowPass[AZAUDIO_REVERB_DELAY_COUNT];
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

// Returns the root mean square (RMS) loudness
int azaRms(const float *input, float *output, azaRmsData *data, int frames, int channels);

// Simple distortion to smooth harsh peaking
int azaCubicLimiter(const float *input, float *output, int frames, int channels);

/*  gain is in db
	NOTE: This limiter increases latency by AZAUDIO_LOOKAHEAD_SAMPLES samples     */
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

#endif // AZAUDIO_DSP_H
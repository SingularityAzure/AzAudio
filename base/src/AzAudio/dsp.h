/*
	File: dps.h
	Author: Philip Haynes
	structs and functions for digital signal processing
*/

#ifndef AZAUDIO_DSP_H
#define AZAUDIO_DSP_H

#include <stdlib.h>

#include "header_utils.h"
#include "math.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AZAUDIO_RMS_SAMPLES 128
#define AZAUDIO_LOOKAHEAD_SAMPLES 128
// The duration of transitions between the variable parameter values
#define AZAUDIO_SAMPLER_TRANSITION_FRAMES 128


// TODO: The most extreme setups can have 20 channels, but there may be no way to actually use that many channels effectively without a way to distinguish them (and the below are all you can get from the MMDevice API on Windows afaik). Should there be more information available, this part of the API will grow.
// TODO: Find a way to get more precise speaker placement information, if that's even possible.
/* These roughly correspond to the following physical positions.
Floor:
    6 2 7
  0       1
 9    H    10
  4   8   5

Ceiling:
  12 13 14
     H
  15 16 17
*/
enum azaPosition {
	AZA_POS_LEFT_FRONT         = 0,
	AZA_POS_RIGHT_FRONT        = 1,
	AZA_POS_CENTER_FRONT       = 2,
	AZA_POS_SUBWOOFER          = 3,
	AZA_POS_LEFT_BACK          = 4,
	AZA_POS_RIGHT_BACK         = 5,
	AZA_POS_LEFT_CENTER_FRONT  = 6,
	AZA_POS_RIGHT_CENTER_FRONT = 7,
	AZA_POS_CENTER_BACK        = 8,
	AZA_POS_LEFT_SIDE          = 9,
	AZA_POS_RIGHT_SIDE         =10,
	AZA_POS_CENTER_TOP         =11,
	AZA_POS_LEFT_FRONT_TOP     =12,
	AZA_POS_CENTER_FRONT_TOP   =13,
	AZA_POS_RIGHT_FRONT_TOP    =14,
	AZA_POS_LEFT_BACK_TOP      =15,
	AZA_POS_CENTER_BACK_TOP    =16,
	AZA_POS_RIGHT_BACK_TOP     =17,
};
// NOTE: This is more than we should ever see in reality, and definitely more than can be uniquely represented by the above positions. We're reserving more for later.
#define AZA_MAX_CHANNEL_POSITIONS 23
#define AZA_POS_ENUM_COUNT (AZA_POS_RIGHT_BACK_TOP+1)

typedef struct azaChannelLayout {
	uint8_t count;
	uint8_t positions[AZA_MAX_CHANNEL_POSITIONS];
} azaChannelLayout;


// Buffer used by DSP functions for their input/output
typedef struct azaBuffer {
	// actual read/write-able data
	// one frame is a single sample from each channel, one after the other
	float *samples;
	// how many samples there are in a single channel
	uint32_t frames;
	// distance between samples from one channel in number of floats
	uint32_t stride;
	// how many channels are stored in this buffer for user-created buffers
	// or how many channels should be accessed by DSP functions
	uint32_t channels;
	// samples per second, used by DSP functions that rely on timing
	uint32_t samplerate;
} azaBuffer;
// You must first set frames and channels before calling this to allocate samples.
// If samples are externally-managed, you don't have to do this.
int azaBufferInit(azaBuffer *data);
int azaBufferDeinit(azaBuffer *data);

// Mixes src into the existing contents of dst
void azaBufferMix(azaBuffer dst, float volumeDst, azaBuffer src, float volumeSrc);

// Copies the contents of one channel of src into dst
void azaBufferCopyChannel(azaBuffer dst, uint32_t channelDst, azaBuffer src, uint32_t channelSrc);

static inline azaBuffer azaBufferOneSample(float *sample, uint32_t samplerate) {
	return AZA_CLITERAL(azaBuffer) {
		/* .samples = */ sample,
		/* .frames = */ 1,
		/* .stride = */ 1,
		/* .channels = */ 1,
		/* .samplerate = */ samplerate,
	};
}


typedef enum azaDSPKind {
	AZA_DSP_NONE=0,
	AZA_DSP_RMS,
	AZA_DSP_FILTER,
	AZA_DSP_LOOKAHEAD_LIMITER,
	AZA_DSP_COMPRESSOR,
	AZA_DSP_DELAY,
	AZA_DSP_REVERB,
	AZA_DSP_SAMPLER,
	AZA_DSP_GATE,
} azaDSPKind;

// Generic interface to all the DSP datas
typedef struct azaDSPData {
	azaDSPKind kind;
	uint32_t structSize;
	struct azaDSPData *pNext;
} azaDSPData;
int azaDSP(azaBuffer buffer, azaDSPData *data);


typedef struct azaRmsData {
	azaDSPData header;
	float squared;
	float buffer[AZAUDIO_RMS_SAMPLES];
	int index;
} azaRmsData;
void azaRmsDataInit(azaRmsData *data);
int azaRms(azaBuffer buffer, azaRmsData *data);


typedef enum azaFilterKind {
	AZA_FILTER_HIGH_PASS,
	AZA_FILTER_LOW_PASS,
	AZA_FILTER_BAND_PASS,
} azaFilterKind;

typedef struct azaFilterData {
	azaDSPData header;
	float outputs[2];

	// User configuration

	azaFilterKind kind;
	// Cutoff frequency in Hz
	float frequency;
	// Blends the effect output with the dry signal where 1 is fully dry and 0 is fully wet.
	float dryMix;
} azaFilterData;
void azaFilterDataInit(azaFilterData *data);
int azaFilter(azaBuffer buffer, azaFilterData *data);



int azaCubicLimiter(azaBuffer buffer);



// NOTE: This limiter increases latency by AZAUDIO_LOOKAHEAD_SAMPLES samples
typedef struct azaLookaheadLimiterData {
	azaDSPData header;
	float gainBuffer[AZAUDIO_LOOKAHEAD_SAMPLES];
	float valBuffer[AZAUDIO_LOOKAHEAD_SAMPLES];
	int index;
	float sum;

	// User configuration

	// input gain in dB
	float gainInput;
	// output gain in dB (should never peak higher than this)
	float gainOutput;
} azaLookaheadLimiterData;
void azaLookaheadLimiterDataInit(azaLookaheadLimiterData *data);
int azaLookaheadLimiter(azaBuffer buffer, azaLookaheadLimiterData *data);



typedef struct azaCompressorData {
	azaDSPData header;
	azaRmsData rmsData;
	float attenuation;
	float gain; // For monitoring/debugging

	// User configuration

	// Activation threshold in dB
	float threshold;
	// positive values allow 1/ratio of the overvolume through
	// negative values subtract overvolume*ratio
	float ratio;
	// attack time in ms
	float attack;
	// decay time in ms
	float decay;
} azaCompressorData;
void azaCompressorDataInit(azaCompressorData *data);
int azaCompressor(azaBuffer buffer, azaCompressorData *data);



typedef struct azaDelayData {
	azaDSPData header;
	float *buffer; // Must be dynamically-allocated to allow different time spans
	size_t capacity;
	// Needs to be kept track of to handle the resizing of buffer gracefully
	size_t delaySamples;
	size_t index;

	// User configuration

	// effect gain in dB
	float gain;
	// dry gain in dB
	float gainDry;
	// delay time in ms
	float delay;
	// 0 to 1 multiple of output feeding back into input
	float feedback;
	// You can provide a chain of effects to operate on the wet output
	azaDSPData *wetEffects;
} azaDelayData;
void azaDelayDataInit(azaDelayData *data);
void azaDelayDataDeinit(azaDelayData *data);
int azaDelay(azaBuffer buffer, azaDelayData *data);



#define AZAUDIO_REVERB_DELAY_COUNT 15
typedef struct azaReverbData {
	azaDSPData header;
	azaDelayData delayDatas[AZAUDIO_REVERB_DELAY_COUNT];
	azaFilterData filterDatas[AZAUDIO_REVERB_DELAY_COUNT];

	// User configuration

	// effect gain in dB
	float gain;
	// dry gain in dB
	float gainDry;
	// value affecting reverb feedback, roughly in the range of 1 to 100 for reasonable results
	float roomsize;
	// value affecting damping of high frequencies, roughly in the range of 1 to 5
	float color;
	// delay for first reflections in ms
	float delay;
} azaReverbData;
void azaReverbDataInit(azaReverbData *data);
void azaReverbDataDeinit(azaReverbData *data);
int azaReverb(azaBuffer buffer, azaReverbData *data);



typedef struct azaSamplerData {
	azaDSPData header;
	float frame;
	float s; // Smooth speed
	float g; // Smooth gain

	// User configuration

	// buffer containing the sound we're sampling
	azaBuffer *buffer;
	// playback speed as a multiple where 1 is full speed
	float speed;
	// volume of effect in dB
	float gain;
} azaSamplerData;
int azaSamplerDataInit(azaSamplerData *data);
int azaSampler(azaBuffer buffer, azaSamplerData *data);



typedef struct azaGateData {
	azaDSPData header;
	azaRmsData rms;
	float attenuation;
	float gain;

	// User configuration

	// cutoff threshold in dB
	float threshold;
	// attack time in ms
	float attack;
	// decay time in ms
	float decay;
	// Any effects to apply to the activation signal
	azaDSPData *activationEffects;
} azaGateData;
void azaGateDataInit(azaGateData *data);
int azaGate(azaBuffer buffer, azaGateData *data);



typedef struct azaKernel {
	// if this is 1, we only store half of the actual table
	int isSymmetrical;
	// length of the kernel, which is half of the actual length if we're symmetrical
	float length;
	// How many samples there are between an interval of length 1
	float scale;
	// total size of table, which is length * scale
	uint32_t size;
	float *table;
} azaKernel;

// Creates a blank kernel
void azaKernelInit(azaKernel *kernel, int isSymmetrical, float length, float scale);
void azaKernelDeinit(azaKernel *kernel);

float azaKernelSample(azaKernel *kernel, float x);

// Makes a lanczos kernel. resolution is the number of samples between zero crossings
void azaKernelMakeLanczos(azaKernel *kernel, float resolution, float radius);


float azaSampleWithKernel(float *src, int stride, int minFrame, int maxFrame, azaKernel *kernel, float pos);

// Performs resampling of src into dst with the given scaling factor and kernel.
// srcFrames is not actually needed here because the sampleable extent is provided by srcFrameMin and srcFrameMax, but for this description it refers to how many samples within src are considered the "meat" of the signal (excluding padding carried over from the last iteration of resampling a stream).
// factor is the scaling ratio (defined roughly as `srcFrames / dstFrames`), passed in explicitly because the exact desired ratio may not be represented accurately by a ratio of the length of two small buffers. For no actual time scaling, this ratio should be perfectly represented by `srcSamplerate / dstSamplerate`.
// src should point at least `-srcFrameMin` frames into an existing source buffer with a total extent of `srcFrameMax-srcFrameMin`.
// srcFrameMin and srcFrameMax allow the accessible extent of src to go outside of the given 0...srcFrames extent, since that's required for perfect resampling of chunks of a stream (while accepting some latency). Ideally, srcFrameMin would be `-kernel->size` and srcFrameMax would be `srcFrames+kernel->size` for a symmetric kernel. For a non-symmetric kernel, srcFrameMin can be 0, and srcFrameMax would still be srcFrames+kernel->size. For two isolated buffers, srcFrameMin should be 0 and srcFrameMax should be srcFrames. Any samples outside of this extent will be considered to be zeroes.
// srcSampleOffset should be in the range 0 to 1
void azaResample(azaKernel *kernel, float factor, float *dst, int dstStride, int dstFrames, float *src, int srcStride, int srcFrameMin, int srcFrameMax, float srcSampleOffset);

// Same as azaResample, except the resampled values are added to dst instead of replacing them. Every sample is multiplied by amp before being added.
void azaResampleAdd(azaKernel *kernel, float factor, float amp, float *dst, int dstStride, int dstFrames, float *src, int srcStride, int srcFrameMin, int srcFrameMax, float srcSampleOffset);


typedef struct azaWorld {
	// Position of our ears
	azaVec3 origin;
	// Must be an orthogonal matrix
	azaMat3 orientation;
	// Speed of sound in units per second.
	// Default: 343.0f (speed of sound in dry air at 20C in m/s)
	float speedOfSound;
} azaWorld;
extern azaWorld azaWorldDefault;

// Does simple angle-based spatialization of the source to map it to the channel layout.
// world can be NULL, indicating to use azaWorldDefault.
// Adds its sound to the existing signal in dstBuffer
void azaMixChannelsSimple(azaBuffer dstBuffer, azaChannelLayout dstChannelLayout, azaBuffer srcBuffer, azaVec3 srcPosStart, float srcAmpStart, azaVec3 srcPosEnd, float srcAmpEnd, const azaWorld *world);



#ifdef __cplusplus
}
#endif

#endif // AZAUDIO_DSP_H
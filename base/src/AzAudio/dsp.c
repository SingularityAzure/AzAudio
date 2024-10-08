/*
	File: dsp.c
	Author: Philip Haynes
*/

#include "dsp.h"

#include "AzAudio/math.h"
#include "error.h"
#include "helpers.h"

// Good ol' MSVC causing problems like always. Never change, MSVC... never change.
#ifdef _MSC_VER
#define AZAUDIO_NO_THREADS_H
#define thread_local __declspec( thread )
#endif

#include <stdlib.h>
#include <string.h>
#ifndef AZAUDIO_NO_THREADS_H
#include <threads.h>
#endif
#include <assert.h>
#include <stdalign.h>


//
//
// BIG TODO: Replace asserts with error reporting when they can be caused by user inputs
//
//


#define AZA_MAX_SIDE_BUFFERS 64
thread_local azaBuffer sideBufferPool[AZA_MAX_SIDE_BUFFERS] = {{0}};
thread_local size_t sideBufferCapacity[AZA_MAX_SIDE_BUFFERS] = {0};
thread_local size_t sideBuffersInUse = 0;

azaKernel azaKernelDefaultLanczos;

azaWorld azaWorldDefault;

azaBuffer azaPushSideBuffer(uint32_t frames, uint32_t channels, uint32_t samplerate) {
	assert(sideBuffersInUse < AZA_MAX_SIDE_BUFFERS);
	azaBuffer *buffer = &sideBufferPool[sideBuffersInUse];
	size_t *capacity = &sideBufferCapacity[sideBuffersInUse];
	size_t capacityNeeded = frames * channels;
	if (*capacity < capacityNeeded) {
		if (*capacity) {
			azaBufferDeinit(buffer);
		}
	}
	buffer->frames = frames;
	buffer->stride = channels;
	buffer->channels.count = channels;
	buffer->samplerate = samplerate;
	if (*capacity < capacityNeeded) {
		azaBufferInit(buffer);
		*capacity = capacityNeeded;
	}
	sideBuffersInUse++;
	return *buffer;
}

azaBuffer azaPushSideBufferZero(uint32_t frames, uint32_t channels, uint32_t samplerate) {
	azaBuffer buffer = azaPushSideBuffer(frames, channels, samplerate);
	memset(buffer.samples, 0, sizeof(float) * frames * channels);
	return buffer;
}

azaBuffer azaPushSideBufferCopy(azaBuffer src) {
	azaBuffer result = azaPushSideBuffer(src.frames, src.channels.count, src.samplerate);
	azaBufferCopy(result, src);
	return result;
}

void azaPopSideBuffer() {
	assert(sideBuffersInUse > 0);
	sideBuffersInUse--;
}

void azaPopSideBuffers(uint8_t count) {
	assert(sideBuffersInUse >= count);
	sideBuffersInUse -= count;
}



static int azaCheckBuffer(azaBuffer buffer) {
	if (buffer.samples == NULL) {
		return AZA_ERROR_NULL_POINTER;
	}
	if (buffer.channels.count < 1) {
		return AZA_ERROR_INVALID_CHANNEL_COUNT;
	}
	if (buffer.frames < 1) {
		return AZA_ERROR_INVALID_FRAME_COUNT;
	}
	return AZA_SUCCESS;
}



int azaBufferInit(azaBuffer *data) {
	if (data->frames < 1) {
		data->samples = NULL;
		return AZA_ERROR_INVALID_FRAME_COUNT;
	}
	data->samples = (float*)aza_calloc(data->frames * data->channels.count, sizeof(float));
	data->stride = data->channels.count;
	return AZA_SUCCESS;
}

int azaBufferDeinit(azaBuffer *data) {
	if (data->samples != NULL) {
		aza_free(data->samples);
		return AZA_SUCCESS;
	}
	return AZA_ERROR_NULL_POINTER;
}

void azaBufferZero(azaBuffer buffer) {
	if (buffer.samples && buffer.frames && buffer.channels.count) {
		if AZA_LIKELY(buffer.channels.count == buffer.stride) {
			memset(buffer.samples, 0, sizeof(float) * buffer.frames * buffer.channels.count);
		} else {
			for (uint32_t i = 0; i < buffer.frames * buffer.stride; i += buffer.stride) {
				for (uint8_t c = 0; c < buffer.channels.count; c++) {
					buffer.samples[i + c] = 0.0f;
				}
			}
		}
	}
}


void azaBufferMix(azaBuffer dst, float volumeDst, azaBuffer src, float volumeSrc) {
	assert(dst.frames == src.frames);
	assert(dst.channels.count == src.channels.count);
	uint32_t channels = dst.channels.count;
	if AZA_UNLIKELY(volumeDst == 1.0f && volumeSrc == 0.0f) {
		return;
	} else if AZA_UNLIKELY(volumeDst == 0.0f && volumeSrc == 0.0f) {
		for (uint32_t i = 0; i < dst.frames; i++) {
			for (uint32_t c = 0; c < channels; c++) {
				dst.samples[i * dst.stride + c] = 0.0f;
			}
		}
	} else if (volumeDst == 1.0f && volumeSrc == 1.0f) {
		for (uint32_t i = 0; i < dst.frames; i++) {
			for (uint32_t c = 0; c < channels; c++) {
				dst.samples[i * dst.stride + c] = dst.samples[i * dst.stride + c] + src.samples[i * src.stride + c];
			}
		}
	} else if AZA_LIKELY(volumeDst == 1.0f) {
		for (uint32_t i = 0; i < dst.frames; i++) {
			for (uint32_t c = 0; c < channels; c++) {
				dst.samples[i * dst.stride + c] = dst.samples[i * dst.stride + c] + src.samples[i * src.stride + c] * volumeSrc;
			}
		}
	} else if (volumeSrc == 1.0f) {
		for (uint32_t i = 0; i < dst.frames; i++) {
			for (uint32_t c = 0; c < channels; c++) {
				dst.samples[i * dst.stride + c] = dst.samples[i * dst.stride + c] * volumeDst + src.samples[i * src.stride + c];
			}
		}
	} else {
		for (uint32_t i = 0; i < dst.frames; i++) {
			for (uint32_t c = 0; c < channels; c++) {
				dst.samples[i * dst.stride + c] = dst.samples[i * dst.stride + c] * volumeDst + src.samples[i * src.stride + c] * volumeSrc;
			}
		}
	}
}

void azaBufferMixFade(azaBuffer dst, float volumeDstStart, float volumeDstEnd, azaBuffer src, float volumeSrcStart, float volumeSrcEnd) {
	if (volumeDstStart == volumeDstEnd && volumeSrcStart == volumeSrcEnd) {
		azaBufferMix(dst, volumeDstStart, src, volumeSrcStart);
		return;
	}
	assert(dst.frames == src.frames);
	assert(dst.channels.count == src.channels.count);
	uint32_t channels = dst.channels.count;
	float volumeDstDelta = volumeDstEnd - volumeDstStart;
	float volumeSrcDelta = volumeSrcEnd - volumeSrcStart;
	float framesF = (float)dst.frames;
	if (volumeDstDelta == 0.0f) {
		if (volumeDstStart == 1.0f) {
			for (uint32_t i = 0; i < dst.frames; i++) {
				float t = (float)i / framesF;
				float volumeSrc = volumeSrcStart + volumeSrcDelta * t;
				for (uint32_t c = 0; c < channels; c++) {
					dst.samples[i * dst.stride + c] = dst.samples[i * dst.stride + c] + src.samples[i * src.stride + c] * volumeSrc;
				}
			}
		} else {
			for (uint32_t i = 0; i < dst.frames; i++) {
				float t = (float)i / framesF;
				float volumeSrc = volumeSrcStart + volumeSrcDelta * t;
				for (uint32_t c = 0; c < channels; c++) {
					dst.samples[i * dst.stride + c] = dst.samples[i * dst.stride + c] * volumeDstStart + src.samples[i * src.stride + c] * volumeSrc;
				}
			}
		}
	} else {
		for (uint32_t i = 0; i < dst.frames; i++) {
			float t = (float)i / framesF;
			float volumeDst = volumeDstStart + volumeDstDelta * t;
			float volumeSrc = volumeSrcStart + volumeSrcDelta * t;
			for (uint32_t c = 0; c < channels; c++) {
				dst.samples[i * dst.stride + c] = dst.samples[i * dst.stride + c] * volumeDst + src.samples[i * src.stride + c] * volumeSrc;
			}
		}
	}
}

void azaBufferCopy(azaBuffer dst, azaBuffer src) {
	assert(dst.frames == src.frames);
	assert(dst.channels.count == src.channels.count);
	if (dst.channels.count == dst.stride && src.channels.count == src.stride) {
		memcpy(dst.samples, src.samples, sizeof(float) * src.frames * src.channels.count);
	} else {
		for (uint32_t i = 0; i < src.frames; i++) {
			for (uint8_t c = 0; c < src.channels.count; c++) {
				dst.samples[i * dst.stride + c] = src.samples[i * src.stride + c];
			}
		}
	}
}

void azaBufferCopyChannel(azaBuffer dst, uint8_t channelDst, azaBuffer src, uint8_t channelSrc) {
	assert(dst.frames == src.frames);
	assert(channelDst < dst.channels.count);
	assert(channelSrc < src.channels.count);
	if (dst.stride == 1 && src.stride == 1) {
		memcpy(dst.samples, src.samples, sizeof(float) * dst.frames);
	} else if (dst.stride == 1) {
		for (uint32_t i = 0; i < dst.frames; i++) {
			dst.samples[i] = src.samples[i * src.stride + channelSrc];
		}
	} else if (src.stride == 1) {
		for (uint32_t i = 0; i < dst.frames; i++) {
			dst.samples[i * dst.stride + channelDst] = src.samples[i];
		}
	} else {
		for (uint32_t i = 0; i < dst.frames; i++) {
			dst.samples[i * dst.stride + channelDst] = src.samples[i * src.stride + channelSrc];
		}
	}
}



int azaProcessDSP(azaBuffer buffer, azaDSP *data) {
	switch (data->kind) {
		case AZA_DSP_RMS: return azaProcessRMS(buffer, (azaRMS*)data);
		case AZA_DSP_FILTER: return azaProcessFilter(buffer, (azaFilter*)data);
		case AZA_DSP_LOOKAHEAD_LIMITER: return azaProcessLookaheadLimiter(buffer, (azaLookaheadLimiter*)data);
		case AZA_DSP_COMPRESSOR: return azaProcessCompressor(buffer, (azaCompressor*)data);
		case AZA_DSP_DELAY: return azaProcessDelay(buffer, (azaDelay*)data);
		case AZA_DSP_REVERB: return azaProcessReverb(buffer, (azaReverb*)data);
		case AZA_DSP_SAMPLER: return azaProcessSampler(buffer, (azaSampler*)data);
		case AZA_DSP_GATE: return azaProcessGate(buffer, (azaGate*)data);
		default: return AZA_ERROR_INVALID_DSP_STRUCT;
	}
}



void azaOpAdd(float *lhs, float rhs) {
	*lhs += rhs;
}
void azaOpMax(float *lhs, float rhs) {
	*lhs = AZA_MAX(*lhs, rhs);
}



static void azaDSPChannelDataInit(azaDSPChannelData *data, uint8_t channelCapInline, uint32_t size, uint8_t alignment) {
	data->capInline = channelCapInline;
	data->capAdditional = 0;
	data->countActive = 0;
	data->alignment = alignment;
	// NOTE: This is probably already aligned, but on the off chance that it's not, we'll handle it.
	data->size = (uint32_t)aza_align(size, alignment);
	data->additional = NULL;
}

static void azaDSPChannelDataDeinit(azaDSPChannelData *data) {
	if (data->additional) {
		aza_free(data->additional);
		data->additional = NULL;
	}
	data->capAdditional = 0;
}

static void azaEnsureChannels(azaDSPChannelData *data, uint8_t channelCount) {
	if (channelCount > data->capInline) {
		uint8_t channelCountAdditional = channelCount - data->capInline;
		if (channelCountAdditional > data->capAdditional) {
			void *newData = aza_calloc(channelCountAdditional, data->size);
			if (data->additional) {
				memcpy(newData, data->additional, data->capAdditional * data->size);
				aza_free(data->additional);
			}
			data->additional = newData;
			data->capAdditional = channelCountAdditional;
		}
	}
}

static void* azaGetChannelData(azaDSPChannelData *data, uint8_t channel) {
	void *result;
	// uint8_t init = channel >= data->countActive;
	if (channel >= data->capInline) {
		channel -= data->capInline;
		assert(channel < data->capAdditional);
		result = (char*)data->additional + channel * data->size;
	} else {
		result = (void*)(aza_align((uint64_t)&data->additional + 8, data->alignment) + channel * data->size);
	}
	// if (init) {
	// 	memset(result, 0, data->size);
	// }
	return result;
}



#define AZA_RMS_INLINE_BUFFER_SIZE 256

static inline uint32_t azaRMSGetBufferCapNeeded(azaRMSConfig config, uint8_t channelCapInline) {
	return (uint32_t)aza_grow(AZA_RMS_INLINE_BUFFER_SIZE, config.windowSamples * AZA_MAX(channelCapInline, 1), 32);
}

uint32_t azaRMSGetAllocSize(azaRMSConfig config, uint8_t channelCapInline) {
	size_t size = sizeof(azaRMS);
	size = azaAddSizeWithAlign(size, channelCapInline * sizeof(azaRMSChannelData), alignof(azaRMSChannelData));
	uint32_t bufferCapNeeded = azaRMSGetBufferCapNeeded(config, channelCapInline);
	if (bufferCapNeeded <= AZA_RMS_INLINE_BUFFER_SIZE) {
		size = aza_align(size + AZA_RMS_INLINE_BUFFER_SIZE * sizeof(float), alignof(azaRMS));
	}
	return (uint32_t)size;
}

void azaRMSInit(azaRMS *data, uint32_t allocSize, azaRMSConfig config, uint8_t channelCapInline) {
	data->header.kind = AZA_DSP_RMS;
	data->header.structSize = allocSize;
	data->config = config;
	azaDSPChannelDataInit(&data->channelData, channelCapInline, sizeof(azaRMSChannelData), alignof(azaRMSChannelData));

	data->bufferCap = azaRMSGetBufferCapNeeded(config, channelCapInline);
	if (data->bufferCap > AZA_RMS_INLINE_BUFFER_SIZE) {
		data->buffer = aza_calloc(data->bufferCap, sizeof(float));
	} else {
		data->buffer = (float*)((char*)&data->buffer + sizeof(float*) + sizeof(azaDSPChannelData) + channelCapInline * sizeof(azaRMSChannelData));
	}
}

void azaRMSDeinit(azaRMS *data) {
	azaDSPChannelDataDeinit(&data->channelData);
	if (data->bufferCap > AZA_RMS_INLINE_BUFFER_SIZE) {
		aza_free(data->buffer);
	}
}

azaRMS* azaMakeRMS(azaRMSConfig config, uint8_t channelCapInline) {
	uint32_t size = azaRMSGetAllocSize(config, channelCapInline);
	azaRMS *result = aza_calloc(1, size);
	azaRMSInit(result, size, config, channelCapInline);
	return result;
}

void azaFreeRMS(azaRMS *data) {
	azaRMSDeinit(data);
	aza_free(data);
}

static void azaHandleRMSBuffer(azaRMS *data, uint8_t channels) {
	if (data->bufferCap < data->config.windowSamples * channels) {
		uint32_t newBufferCap = (uint32_t)aza_grow(data->bufferCap, data->config.windowSamples * channels, 32);
		float *newBuffer = aza_calloc(newBufferCap, sizeof(float));
		if (data->bufferCap > AZA_RMS_INLINE_BUFFER_SIZE) {
			aza_free(data->buffer);
		}
		data->bufferCap = newBufferCap;
		data->buffer = newBuffer;
	}
}

int azaProcessRMSCombined(azaBuffer dst, azaBuffer src, azaRMS *data, fp_azaOp op) {
	if (data == NULL) {
		return AZA_ERROR_NULL_POINTER;
	} else {
		int err = azaCheckBuffer(dst);
		if (err) return err;
		err = azaCheckBuffer(src);
		if (err) return err;
	}
	azaHandleRMSBuffer(data, 1);
	azaEnsureChannels(&data->channelData, 1);
	azaRMSChannelData *channelData = azaGetChannelData(&data->channelData, 0);
	float *channelBuffer = data->buffer;
	for (size_t i = 0; i < src.frames; i++) {
		channelData->squaredSum -= channelBuffer[data->index];
		channelBuffer[data->index] = 0.0f;
		for (size_t c = 0; c < src.channels.count; c++) {
			op(&channelBuffer[data->index], azaSqr(src.samples[i * src.stride + c]));
		}
		channelData->squaredSum += channelBuffer[data->index];
		// Deal with potential rounding errors making sqrtf emit NaNs
		if (channelData->squaredSum < 0.0f) channelData->squaredSum = 0.0f;
		dst.samples[i * dst.stride] = sqrtf(channelData->squaredSum/(data->config.windowSamples * src.channels.count));
		if (++data->index >= data->config.windowSamples)
			data->index = 0;
	}
	if (data->header.pNext) {
		return azaProcessDSP(dst, data->header.pNext);
	}
	return AZA_SUCCESS;
}

int azaProcessRMS(azaBuffer buffer, azaRMS *data) {
	if (data == NULL) {
		return AZA_ERROR_NULL_POINTER;
	} else {
		int err = azaCheckBuffer(buffer);
		if (err) return err;
	}
	azaHandleRMSBuffer(data, buffer.channels.count);
	azaEnsureChannels(&data->channelData, buffer.channels.count);
	for (uint8_t c = 0; c < buffer.channels.count; c++) {
		azaRMSChannelData *channelData = azaGetChannelData(&data->channelData, c);
		float *channelBuffer = &data->buffer[data->config.windowSamples * c];

		for (uint32_t i = 0; i < buffer.frames; i++) {
			uint32_t s = i * buffer.stride + c;
			channelData->squaredSum -= channelBuffer[data->index];
			channelBuffer[data->index] = azaSqr(buffer.samples[s]);
			channelData->squaredSum += channelBuffer[data->index];
			// Deal with potential rounding errors making sqrtf emit NaNs
			if (channelData->squaredSum < 0.0f) channelData->squaredSum = 0.0f;

			if (++data->index >= data->config.windowSamples)
				data->index = 0;

			buffer.samples[s] = sqrtf(channelData->squaredSum/data->config.windowSamples);
		}
	}
	if (data->header.pNext) {
		return azaProcessDSP(buffer, data->header.pNext);
	}
	return AZA_SUCCESS;
}



static float azaCubicLimiterSample(float sample) {
	if (sample > 1.0f)
		sample = 1.0f;
	else if (sample < -1.0f)
		sample = -1.0f;
	sample = 1.5f * sample - 0.5f * sample * sample * sample;
	return sample;
}

int azaProcessCubicLimiter(azaBuffer buffer) {
	{
		int err = azaCheckBuffer(buffer);
		if (err) return err;
	}
	if (buffer.stride == buffer.channels.count) {
		for (size_t i = 0; i < buffer.frames*buffer.channels.count; i++) {
			buffer.samples[i] = azaCubicLimiterSample(buffer.samples[i]);
		}
	} else {
		for (size_t c = 0; c < buffer.channels.count; c++) {
			for (size_t i = 0; i < buffer.frames; i++) {
				size_t s = i * buffer.stride + c;
				buffer.samples[s] = azaCubicLimiterSample(buffer.samples[s]);
			}
		}
	}
	return AZA_SUCCESS;
}



uint32_t azaLookaheadLimiterGetAllocSize(uint8_t channelCapInline) {
	size_t size = sizeof(azaLookaheadLimiter);
	size = azaAddSizeWithAlign(size, channelCapInline * sizeof(azaLookaheadLimiterChannelData), alignof(azaLookaheadLimiterChannelData));
	return (uint32_t)size;
}

void azaLookaheadLimiterInit(azaLookaheadLimiter *data, uint32_t allocSize, azaLookaheadLimiterConfig config, uint8_t channelCapInline) {
	data->header.kind = AZA_DSP_LOOKAHEAD_LIMITER;
	data->header.structSize = allocSize;
	data->config = config;
	azaDSPChannelDataInit(&data->channelData, channelCapInline, sizeof(azaLookaheadLimiterChannelData), alignof(azaLookaheadLimiterChannelData));
}

void azaLookaheadLimiterDeinit(azaLookaheadLimiter *data) {
	azaDSPChannelDataDeinit(&data->channelData);
}

azaLookaheadLimiter* azaMakeLookaheadLimiter(azaLookaheadLimiterConfig config, uint8_t channelCapInline) {
	uint32_t size = azaLookaheadLimiterGetAllocSize(channelCapInline);
	azaLookaheadLimiter *result = aza_calloc(1, size);
	azaLookaheadLimiterInit(result, size, config, channelCapInline);
	return result;
}

void azaFreeLookaheadLimiter(azaLookaheadLimiter *data) {
	azaLookaheadLimiterDeinit(data);
	aza_free(data);
}

int azaProcessLookaheadLimiter(azaBuffer buffer, azaLookaheadLimiter *data) {
	if (data == NULL) {
		return AZA_ERROR_NULL_POINTER;
	} else {
		int err = azaCheckBuffer(buffer);
		if (err) return err;
	}
	azaEnsureChannels(&data->channelData, buffer.channels.count);
	azaBuffer gainBuffer;
	gainBuffer = azaPushSideBuffer(buffer.frames, 1, buffer.samplerate);
	memset(gainBuffer.samples, 0, sizeof(float) * gainBuffer.frames);
	// TODO: It may be desirable to prevent the subwoofer channel from affecting the rest, and it may want its own independent limiter.
	int index = data->index;
	// Do all the gain calculations and put them into gainBuffer
	for (uint32_t i = 0; i < buffer.frames; i++) {
		for (uint8_t c = 0; c < buffer.channels.count; c++) {
			float sample = azaAbs(buffer.samples[i * buffer.stride + c]);
			gainBuffer.samples[i] = AZA_MAX(sample, gainBuffer.samples[i]);
		}
		float gain = data->config.gainInput;
		float peak = AZA_MAX(aza_amp_to_dbf(gainBuffer.samples[i]) + gain, 0.0f);
		float slope = (peak - data->sum) / AZAUDIO_LOOKAHEAD_SAMPLES;
		if (slope > 0.0f && slope > data->slope) {
			data->slope = slope;
			data->cooldown = AZAUDIO_LOOKAHEAD_SAMPLES;
		} else if (data->cooldown == 0 && data->sum > 0.0f) {
			data->slope = -data->sum / (AZAUDIO_LOOKAHEAD_SAMPLES * 5.0f);
			for (int index2 = 0; index2 < AZAUDIO_LOOKAHEAD_SAMPLES; index2++) {
				float peak2 = data->peakBuffer[(index+index2)%AZAUDIO_LOOKAHEAD_SAMPLES];
				float slope2 = (peak2 - data->sum) / (float)(index2+1);
				if (slope2 > 0.0f && slope2 > data->slope) {
					data->slope = slope2;
					data->cooldown = index2+1;
				}
			}
		} else if (data->cooldown > 0) {
			data->cooldown -= 1;
		}
		data->sum += data->slope;
		if (data->sum < 0.0f) {
			data->slope = 0.0f;
			data->sum = 0.0f;
		}
		data->peakBuffer[index] = peak;
		index = (index+1)%AZAUDIO_LOOKAHEAD_SAMPLES;
		gainBuffer.samples[i] = aza_db_to_ampf(-data->sum);
	}
	float amountInput = aza_db_to_ampf(data->config.gainInput);
	float amountOutput = aza_db_to_ampf(data->config.gainOutput);
	// Apply the gain from gainBuffer to all the channels
	for (uint8_t c = 0; c < buffer.channels.count; c++) {
		azaLookaheadLimiterChannelData *channelData = azaGetChannelData(&data->channelData, c);
		index = data->index;

		for (uint32_t i = 0; i < buffer.frames; i++) {
			uint32_t s = i * buffer.stride + c;
			channelData->valBuffer[index] = buffer.samples[s];
			index = (index+1)%AZAUDIO_LOOKAHEAD_SAMPLES;
			float out = azaClampf(channelData->valBuffer[index] * gainBuffer.samples[i] * amountInput, -1.0f, 1.0f);
			buffer.samples[s] = out * amountOutput;
		}
	}
	data->index = index;
	data->channelData.countActive = buffer.channels.count;
	azaPopSideBuffer();
	if (data->header.pNext) {
		return azaProcessDSP(buffer, data->header.pNext);
	}
	return AZA_SUCCESS;
}



uint32_t azaFilterGetAllocSize(uint8_t channelCapInline) {
	size_t size = sizeof(azaFilter);
	size = azaAddSizeWithAlign(size, channelCapInline * sizeof(azaFilterChannelData), alignof(azaFilterChannelData));
	return (uint32_t)size;
}

void azaFilterInit(azaFilter *data, uint32_t allocSize, azaFilterConfig config, uint8_t channelCapInline) {
	data->header.kind = AZA_DSP_FILTER;
	data->header.structSize = allocSize;
	data->config = config;
	azaDSPChannelDataInit(&data->channelData, channelCapInline, sizeof(azaFilterChannelData), alignof(azaFilterChannelData));
}

void azaFilterDeinit(azaFilter *data) {
	azaDSPChannelDataDeinit(&data->channelData);
}

azaFilter* azaMakeFilter(azaFilterConfig config, uint8_t channelCapInline) {
	uint32_t size = azaFilterGetAllocSize(channelCapInline);
	azaFilter *result = aza_calloc(1, size);
	azaFilterInit(result, size, config, channelCapInline);
	return result;
}

void azaFreeFilter(azaFilter *data) {
	azaFilterDeinit(data);
	aza_free(data);
}

int azaProcessFilter(azaBuffer buffer, azaFilter *data) {
	if (data == NULL) {
		return AZA_ERROR_NULL_POINTER;
	} else {
		int err = azaCheckBuffer(buffer);
		if (err) return err;
	}
	azaEnsureChannels(&data->channelData, buffer.channels.count);
	float amount = azaClampf(1.0f - data->config.dryMix, 0.0f, 1.0f);
	float amountDry = azaClampf(data->config.dryMix, 0.0f, 1.0f);
	for (uint8_t c = 0; c < buffer.channels.count; c++) {
		azaFilterChannelData *channelData = azaGetChannelData(&data->channelData, c);

		switch (data->config.kind) {
			case AZA_FILTER_HIGH_PASS: {
				float decay = azaClampf(expf(-AZA_TAU * (data->config.frequency / (float)buffer.samplerate)), 0.0f, 1.0f);
				for (uint32_t i = 0; i < buffer.frames; i++) {
					uint32_t s = i * buffer.stride + c;
					channelData->outputs[0] = buffer.samples[s] + decay * (channelData->outputs[0] - buffer.samples[s]);
					buffer.samples[s] = (buffer.samples[s] - channelData->outputs[0]) * amount + buffer.samples[s] * amountDry;
				}
			} break;
			case AZA_FILTER_LOW_PASS: {
				float decay = azaClampf(expf(-AZA_TAU * (data->config.frequency / (float)buffer.samplerate)), 0.0f, 1.0f);
				for (uint32_t i = 0; i < buffer.frames; i++) {
					uint32_t s = i * buffer.stride + c;
					channelData->outputs[0] = buffer.samples[s] + decay * (channelData->outputs[0] - buffer.samples[s]);
					buffer.samples[s] = channelData->outputs[0] * amount + buffer.samples[s] * amountDry;
				}
			} break;
			case AZA_FILTER_BAND_PASS: {
				float decayLow = azaClampf(expf(-AZA_TAU * (data->config.frequency / (float)buffer.samplerate)), 0.0f, 1.0f);
				float decayHigh = azaClampf(expf(-AZA_TAU * (data->config.frequency / (float)buffer.samplerate)), 0.0f, 1.0f);
				for (uint32_t i = 0; i < buffer.frames; i++) {
					uint32_t s = i * buffer.stride + c;
					channelData->outputs[0] = buffer.samples[s] + decayLow * (channelData->outputs[0] - buffer.samples[s]);
					channelData->outputs[1] = channelData->outputs[0] + decayHigh * (channelData->outputs[1] - channelData->outputs[0]);
					buffer.samples[s] = (channelData->outputs[0] - channelData->outputs[1]) * 2.0f * amount + buffer.samples[s] * amountDry;
				}
			} break;
		}
	}
	data->channelData.countActive = buffer.channels.count;
	if (data->header.pNext) {
		return azaProcessDSP(buffer, data->header.pNext);
	}
	return AZA_SUCCESS;
}



uint32_t azaCompressorGetAllocSize(uint8_t channelCapInline) {
	size_t size = sizeof(azaCompressor) - sizeof(azaRMS);
	size = azaAddSizeWithAlign(size, azaRMSGetAllocSize((azaRMSConfig) { 128 }, channelCapInline), alignof(azaRMS));
	return (uint32_t)size;
}

void azaCompressorInit(azaCompressor *data, uint32_t allocSize, azaCompressorConfig config, uint8_t channelCapInline) {
	data->header.kind = AZA_DSP_COMPRESSOR;
	data->header.structSize = allocSize;
	data->config = config;
	azaRMSConfig rmsConfig = (azaRMSConfig) { 128 };
	azaRMSInit(&data->rms, azaRMSGetAllocSize(rmsConfig, 1), rmsConfig, 1);
}

void azaCompressorDeinit(azaCompressor *data) {
	azaRMSDeinit(&data->rms);
}

azaCompressor* azaMakeCompressor(azaCompressorConfig config, uint8_t channelCapInline) {
	uint32_t size = azaCompressorGetAllocSize(channelCapInline);
	azaCompressor *result = aza_calloc(1, size);
	azaCompressorInit(result, size, config, channelCapInline);
	return result;
}

void azaFreeCompressor(azaCompressor *data) {
	azaCompressorDeinit(data);
	aza_free(data);
}

int azaProcessCompressor(azaBuffer buffer, azaCompressor *data) {
	if (data == NULL) {
		return AZA_ERROR_NULL_POINTER;
	} else {
		int err = azaCheckBuffer(buffer);
		if (err) return err;
	}
	azaBuffer rmsBuffer = azaPushSideBuffer(buffer.frames, 1, buffer.samplerate);
	azaProcessRMSCombined(rmsBuffer, buffer, &data->rms, azaOpMax);
	float t = (float)buffer.samplerate / 1000.0f;
	float attackFactor = expf(-1.0f / (data->config.attack * t));
	float decayFactor = expf(-1.0f / (data->config.decay * t));
	float overgainFactor;
	if (data->config.ratio > 1.0f) {
		overgainFactor = (1.0f - 1.0f / data->config.ratio);
	} else if (data->config.ratio < 0.0f) {
		overgainFactor = -data->config.ratio;
	} else {
		overgainFactor = 0.0f;
	}
	for (size_t i = 0; i < buffer.frames; i++) {
		float rms = aza_amp_to_dbf(rmsBuffer.samples[i]);
		if (rms < -120.0f) rms = -120.0f;
		if (rms > data->attenuation) {
			data->attenuation = rms + attackFactor * (data->attenuation - rms);
		} else {
			data->attenuation = rms + decayFactor * (data->attenuation - rms);
		}
		float gain;
		if (data->attenuation > data->config.threshold) {
			gain = overgainFactor * (data->config.threshold - data->attenuation);
		} else {
			gain = 0.0f;
		}
		data->gain = gain;
		float amp = aza_db_to_ampf(gain);
		for (size_t c = 0; c < buffer.channels.count; c++) {
			size_t s = i * buffer.stride + c;
			buffer.samples[s] *= amp;
		}
	}
	azaPopSideBuffer();
	if (data->header.pNext) {
		return azaProcessDSP(buffer, data->header.pNext);
	}
	return AZA_SUCCESS;
}



uint32_t azaDelayGetAllocSize(uint8_t channelCapInline) {
	size_t size = sizeof(azaDelay);
	size = azaAddSizeWithAlign(size, channelCapInline * sizeof(azaDelayChannelData), alignof(azaDelayChannelData));
	return (uint32_t)size;
}

void azaDelayInit(azaDelay *data, uint32_t allocSize, azaDelayConfig config, uint8_t channelCapInline) {
	data->header.kind = AZA_DSP_DELAY;
	data->header.structSize = allocSize;
	data->config = config;
	azaDSPChannelDataInit(&data->channelData, channelCapInline, sizeof(azaDelayChannelData), alignof(azaDelayChannelData));
}

void azaDelayDeinit(azaDelay *data) {
	if (data->buffer) {
		aza_free(data->buffer);
		data->buffer = NULL;
	}
}

azaDelayChannelConfig* azaDelayGetChannelConfig(azaDelay *data, uint8_t channel) {
	azaDelayChannelData *channelData = azaGetChannelData(&data->channelData, channel);
	return &channelData->config;
}

azaDelay* azaMakeDelay(azaDelayConfig config, uint8_t channelCapInline) {
	uint32_t size = azaDelayGetAllocSize(channelCapInline);
	azaDelay *result = aza_calloc(1, size);
	azaDelayInit(result, size, config, channelCapInline);
	return result;
}

void azaFreeDelay(azaDelay *data) {
	azaDelayDeinit(data);
	aza_free(data);
}

static void azaDelayHandleBufferResizes(azaDelay *data, uint32_t samplerate, uint8_t channelCount) {
	azaEnsureChannels(&data->channelData, channelCount);
	uint32_t delaySamplesMax = 0;
	uint32_t perChannelBufferCap = data->bufferCap / channelCount;
	uint8_t realloc = 0;
	for (uint8_t c = 0; c < channelCount; c++) {
		azaDelayChannelData *channelData = azaGetChannelData(&data->channelData, c);
		uint32_t delaySamples = (uint32_t)aza_ms_to_samples(data->config.delay + channelData->config.delay, (float)samplerate);
		if (delaySamples > delaySamplesMax) delaySamplesMax = delaySamples;
		if (channelData->delaySamples >= delaySamples) {
			if (channelData->index > delaySamples) {
				channelData->index = 0;
			}
			channelData->delaySamples = delaySamples;
		} else if (perChannelBufferCap >= delaySamples) {
			channelData->delaySamples = delaySamples;
		} else {
			realloc = 1;
		}
	}
	if (!realloc) return;
	// Have to realloc buffer
	uint32_t newPerChannelBufferCap = (uint32_t)aza_grow(data->bufferCap / channelCount, delaySamplesMax, 256);
	float *newBuffer = aza_calloc(sizeof(float), newPerChannelBufferCap * channelCount);
	for (uint8_t c = 0; c < channelCount; c++) {
		azaDelayChannelData *channelData = azaGetChannelData(&data->channelData, c);
		float *newChannelBuffer = newBuffer + c * newPerChannelBufferCap;
		if (data->buffer && channelData->delaySamples) {
			memcpy(newChannelBuffer, channelData->buffer, sizeof(float) * channelData->delaySamples);
		}
		channelData->buffer = newChannelBuffer;
		// We also have to set delaySamples since we didn't do it above
		channelData->delaySamples = (uint32_t)aza_ms_to_samples(data->config.delay + channelData->config.delay, (float)samplerate);
	}
	if (data->buffer) {
		aza_free(data->buffer);
	}
	data->buffer = newBuffer;
}

int azaProcessDelay(azaBuffer buffer, azaDelay *data) {
	if (data == NULL) {
		return AZA_ERROR_NULL_POINTER;
	} else {
		int err = azaCheckBuffer(buffer);
		if (err) return err;
	}
	azaDelayHandleBufferResizes(data, buffer.samplerate, buffer.channels.count);
	azaBuffer sideBuffer = azaPushSideBuffer(buffer.frames, buffer.channels.count, buffer.samplerate);
	memset(sideBuffer.samples, 0, sizeof(float) * sideBuffer.frames * sideBuffer.channels.count);
	for (uint8_t c = 0; c < buffer.channels.count; c++) {
		azaDelayChannelData *channelData = azaGetChannelData(&data->channelData, c);
		uint32_t index = channelData->index;
		for (uint32_t i = 0; i < buffer.frames; i++) {
			uint32_t s = i * buffer.stride + c;
			uint8_t c2 = (c + 1) % buffer.channels.count;
			float toAdd = buffer.samples[s] + channelData->buffer[index] * data->config.feedback;
			sideBuffer.samples[i * sideBuffer.stride + c] += toAdd * (1.0f - data->config.pingpong);
			sideBuffer.samples[i * sideBuffer.stride + c2] += toAdd * data->config.pingpong;
			index = (index+1) % channelData->delaySamples;
		}
	}
	if (data->config.wetEffects) {
		int err = azaProcessDSP(sideBuffer, data->config.wetEffects);
		if (err) return err;
	}
	for (uint8_t c = 0; c < buffer.channels.count; c++) {
		azaDelayChannelData *channelData = azaGetChannelData(&data->channelData, c);
		uint32_t index = channelData->index;
		float amount = aza_db_to_ampf(data->config.gain);
		float amountDry = aza_db_to_ampf(data->config.gainDry);
		for (uint32_t i = 0; i < buffer.frames; i++) {
			uint32_t s = i * buffer.stride + c;
			channelData->buffer[index] = sideBuffer.samples[i * sideBuffer.stride + c];
			index = (index+1) % channelData->delaySamples;
			buffer.samples[s] = channelData->buffer[index] * amount + buffer.samples[s] * amountDry;
		}
		channelData->index = index;
	}
	azaPopSideBuffer();
	if (data->header.pNext) {
		return azaProcessDSP(buffer, data->header.pNext);
	}
	return AZA_SUCCESS;
}



uint32_t azaReverbGetAllocSize(uint8_t channelCapInline) {
	size_t size = sizeof(azaReverb) - sizeof(azaDelay);
	size = azaAddSizeWithAlign(size, azaDelayGetAllocSize(channelCapInline), alignof(azaDelay));
	size = azaAddSizeWithAlign(size, AZAUDIO_REVERB_DELAY_COUNT * azaDelayGetAllocSize(channelCapInline), alignof(azaDelay));
	size = azaAddSizeWithAlign(size, AZAUDIO_REVERB_DELAY_COUNT * azaFilterGetAllocSize(channelCapInline), alignof(azaFilter));
	return (uint32_t)size;
}

void azaReverbInit(azaReverb *data, uint32_t allocSize, azaReverbConfig config, uint8_t channelCapInline) {
	data->header.kind = AZA_DSP_REVERB;
	data->header.structSize = allocSize;
	data->config = config;

	uint32_t delayAllocSize = azaDelayGetAllocSize(channelCapInline);
	uint32_t filterAllocSize = azaFilterGetAllocSize(channelCapInline);

	azaDelayInit(&data->inputDelay, delayAllocSize, (azaDelayConfig){
		.gain = 0.0f,
		.gainDry = -INFINITY,
		.delay = config.delay,
		.feedback = 0.0f,
		.wetEffects = NULL,
		.pingpong = 0.0f,
	}, channelCapInline);

	float delays[AZAUDIO_REVERB_DELAY_COUNT] = {
		aza_samples_to_ms(2111, 48000),
		aza_samples_to_ms(2129, 48000),
		aza_samples_to_ms(2017, 48000),
		aza_samples_to_ms(2029, 48000),
		aza_samples_to_ms(1753, 48000),
		aza_samples_to_ms(1733, 48000),
		aza_samples_to_ms(1699, 48000),
		aza_samples_to_ms(1621, 48000),
		aza_samples_to_ms(1447, 48000),
		aza_samples_to_ms(1429, 48000),
		aza_samples_to_ms(1361, 48000),
		aza_samples_to_ms(1319, 48000),
		aza_samples_to_ms(1201, 48000),
		aza_samples_to_ms(1171, 48000),
		aza_samples_to_ms(1129, 48000),
		aza_samples_to_ms(1117, 48000),
		aza_samples_to_ms(1063, 48000),
		aza_samples_to_ms(1051, 48000),
		aza_samples_to_ms(1039, 48000),
		aza_samples_to_ms(1009, 48000),
		aza_samples_to_ms( 977, 48000),
		aza_samples_to_ms( 919, 48000),
		aza_samples_to_ms( 857, 48000),
		aza_samples_to_ms( 773, 48000),
		aza_samples_to_ms( 743, 48000),
		aza_samples_to_ms( 719, 48000),
		aza_samples_to_ms( 643, 48000),
		aza_samples_to_ms( 641, 48000),
		aza_samples_to_ms( 631, 48000),
		aza_samples_to_ms( 619, 48000),
	};
	for (int tap = 0; tap < AZAUDIO_REVERB_DELAY_COUNT; tap++) {
		azaDelay *delay = azaReverbGetDelayTap(data, tap);
		azaFilter *filter = azaReverbGetFilterTap(data, tap);
		azaDelayInit(delay, delayAllocSize, (azaDelayConfig) {
			.gain = 0.0f,
			.gainDry = -INFINITY,
			.delay = delays[tap],
			.feedback = 0.0f,
			.wetEffects = NULL,
			.pingpong = 0.05f,
		}, channelCapInline);
		azaFilterInit(filter, filterAllocSize, (azaFilterConfig) {
			.kind = AZA_FILTER_LOW_PASS,
			.frequency = 1000.0f,
			.dryMix = 0.0f,
		}, channelCapInline);
	}
}

void azaReverbDeinit(azaReverb *data) {
	azaDelayDeinit(&data->inputDelay);
	for (int tap = 0; tap < AZAUDIO_REVERB_DELAY_COUNT; tap++) {
		azaDelay *delay = azaReverbGetDelayTap(data, tap);
		azaFilter *filter = azaReverbGetFilterTap(data, tap);
		azaDelayDeinit(delay);
		azaFilterDeinit(filter);
	}
}

azaDelay* azaReverbGetDelayTap(azaReverb *data, int tap) {
	uint8_t channelCapInline = data->inputDelay.channelData.capInline;
	size_t size = sizeof(azaReverb) - sizeof(azaDelay);
	size = azaAddSizeWithAlign(size, azaDelayGetAllocSize(channelCapInline), alignof(azaDelay));
	size = azaAddSizeWithAlign(size, tap * azaDelayGetAllocSize(channelCapInline), alignof(azaDelay));
	azaDelay *delay = (azaDelay*)azaGetBufferOffset((char*)data, size, alignof(azaDelay));
	return delay;
}

azaFilter* azaReverbGetFilterTap(azaReverb *data, int tap) {
	uint8_t channelCapInline = data->inputDelay.channelData.capInline;
	size_t size = sizeof(azaReverb) - sizeof(azaDelay);
	size = azaAddSizeWithAlign(size, azaDelayGetAllocSize(channelCapInline), alignof(azaDelay));
	size = azaAddSizeWithAlign(size, AZAUDIO_REVERB_DELAY_COUNT * azaDelayGetAllocSize(channelCapInline), alignof(azaDelay));
	size = azaAddSizeWithAlign(size, tap * azaFilterGetAllocSize(channelCapInline), alignof(azaFilter));
	azaFilter *filter = (azaFilter*)azaGetBufferOffset((char*)data, size, alignof(azaFilter));
	return filter;
}

azaReverb* azaMakeReverb(azaReverbConfig config, uint8_t channelCapInline) {
	uint32_t size = azaReverbGetAllocSize(channelCapInline);
	azaReverb *result = aza_calloc(1, size);
	azaReverbInit(result, size, config, channelCapInline);
	return result;
}

void azaFreeReverb(azaReverb *data) {
	azaReverbDeinit(data);
	aza_free(data);
}

int azaProcessReverb(azaBuffer buffer, azaReverb *data) {
	if (data == NULL) {
		return AZA_ERROR_NULL_POINTER;
	} else {
		int err = azaCheckBuffer(buffer);
		if (err) return err;
	}
	azaBuffer inputBuffer = azaPushSideBufferCopy(buffer);
	azaProcessDelay(inputBuffer, &data->inputDelay);
	azaBuffer sideBufferCombined = azaPushSideBufferZero(buffer.frames, buffer.channels.count, buffer.samplerate);
	azaBuffer sideBufferEarly = azaPushSideBuffer(buffer.frames, buffer.channels.count, buffer.samplerate);
	azaBuffer sideBufferDiffuse = azaPushSideBuffer(buffer.frames, buffer.channels.count, buffer.samplerate);
	float feedback = 0.985f - (0.2f / data->config.roomsize);
	float color = data->config.color * 4000.0f;
	float amount = aza_db_to_ampf(data->config.gain);
	float amountDry = aza_db_to_ampf(data->config.gainDry);
	for (int tap = 0; tap < AZAUDIO_REVERB_DELAY_COUNT*2/3; tap++) {
		// TODO: Make feedback depend on delay time such that they all decay in amplitude at the same rate over time
		azaDelay *delay = azaReverbGetDelayTap(data, tap);
		azaFilter *filter = azaReverbGetFilterTap(data, tap);
		delay->config.feedback = feedback;
		filter->config.frequency = color;
		memcpy(sideBufferEarly.samples, inputBuffer.samples, sizeof(float) * buffer.frames * buffer.channels.count);
		azaProcessFilter(sideBufferEarly, filter);
		azaProcessDelay(sideBufferEarly, delay);
		azaBufferMix(sideBufferCombined, 1.0f, sideBufferEarly, 1.0f / (float)AZAUDIO_REVERB_DELAY_COUNT);
	}
	for (int tap = AZAUDIO_REVERB_DELAY_COUNT*2/3; tap < AZAUDIO_REVERB_DELAY_COUNT; tap++) {
		azaDelay *delay = azaReverbGetDelayTap(data, tap);
		azaFilter *filter = azaReverbGetFilterTap(data, tap);
		delay->config.feedback = (float)(tap+AZAUDIO_REVERB_DELAY_COUNT) / (AZAUDIO_REVERB_DELAY_COUNT*2);
		filter->config.frequency = color*4.0f;
		memcpy(sideBufferDiffuse.samples, sideBufferCombined.samples, sizeof(float) * buffer.frames * buffer.channels.count);
		azaBufferCopyChannel(sideBufferDiffuse, 0, sideBufferCombined, 0);
		azaProcessFilter(sideBufferDiffuse, filter);
		azaProcessDelay(sideBufferDiffuse, delay);
		azaBufferMix(sideBufferCombined, 1.0f, sideBufferDiffuse, 1.0f / (float)AZAUDIO_REVERB_DELAY_COUNT);
	}
	azaBufferMix(buffer, amountDry, sideBufferCombined, amount);
	azaPopSideBuffers(4);
	if (data->header.pNext) {
		return azaProcessDSP(buffer, data->header.pNext);
	}
	return AZA_SUCCESS;
}



void azaSamplerInit(azaSampler *data, azaSamplerConfig config) {
	data->header.kind = AZA_DSP_SAMPLER;
	data->header.structSize = azaSamplerGetAllocSize();
	data->config = config;
	data->frame = 0;
	data->s = config.speed;
	// Starting at zero ensures click-free playback no matter what
	data->g = 0.0f;
	// TODO: Probably use envelopes
}

void azaSamplerDeinit(azaSampler *data) {
	// Nothing to do :)
}

azaSampler* azaMakeSampler(azaSamplerConfig config) {
	uint32_t size = azaSamplerGetAllocSize();
	azaSampler *result = aza_calloc(1, size);
	azaSamplerInit(result, config);
	return result;
}

void azaFreeSampler(azaSampler *data) {
	azaSamplerDeinit(data);
	aza_free(data);
}

int azaProcessSampler(azaBuffer buffer, azaSampler *data) {
	if (data == NULL || data->config.buffer == NULL) {
		return AZA_ERROR_NULL_POINTER;
	} else {
		int err = azaCheckBuffer(buffer);
		if (err) return err;
		if (buffer.channels.count != data->config.buffer->channels.count) {
			return AZA_ERROR_CHANNEL_COUNT_MISMATCH;
		}
	}
	float transition = expf(-1.0f / (AZAUDIO_SAMPLER_TRANSITION_FRAMES));
	float samplerateFactor = (float)data->config.buffer->samplerate / (float)buffer.samplerate;
	for (size_t i = 0; i < buffer.frames; i++) {
		data->s = data->config.speed + transition * (data->s - data->config.speed);
		data->g = data->config.gain + transition * (data->g - data->config.gain);

		// Adjust for different samplerates
		float speed = data->s * samplerateFactor;
		float volume = aza_db_to_ampf(data->g);

		for (uint8_t c = 0; c < buffer.channels.count; c++) {
			float sample = 0.0f;
			// TODO: Maybe switch to using the lanczos kernel that we use to resample for the backend
			/* Lanczos
			int t = (int)datum->frame + (int)data->s;
			for (int i = (int)datum->frame-2; i <= t+2; i++) {
				float x = datum->frame - (float)(i);
				sample += datum->buffer->samples[i % datum->buffer->frames] * sinc(x) * sinc(x/3);
			}
			*/

			if (speed <= 1.0f) {
				// Cubic
				float abcd[4];
				int ii = data->frame + (int)data->config.buffer->frames - 2;
				for (int i = 0; i < 4; i++) {
					abcd[i] = data->config.buffer->samples[(ii++ % data->config.buffer->frames) * data->config.buffer->stride + c];
				}
				sample = cubic(abcd[0], abcd[1], abcd[2], abcd[3], data->frameFraction);
			} else {
				// Oversampling
				float total = 0.0f;
				total += data->config.buffer->samples[(data->frame % data->config.buffer->frames) * data->config.buffer->stride + c] * (1.0f - data->frameFraction);
				for (int i = 1; i < (int)speed; i++) {
					total += data->config.buffer->samples[((data->frame + i) % data->config.buffer->frames) * data->config.buffer->stride + c];
				}
				total += data->config.buffer->samples[((data->frame + (int)speed) % data->config.buffer->frames) * data->config.buffer->stride + c] * data->frameFraction;
				sample = total / (float)((int)speed);
			}

			/* Linear
			int t = (int)data->frame + (int)data->s;
			for (int i = (int)data->frame; i <= t+1; i++) {
				float x = data->frame - (float)(i);
				sample += data->config.buffer->samples[i % data->config.buffer->frames] * linc(x);
			}
			*/

			buffer.samples[i * buffer.stride + c] = sample * volume;
		}
		data->frameFraction += speed;
		uint32_t framesToAdd = (uint32_t)data->frameFraction;
		data->frame += framesToAdd;
		data->frameFraction -= framesToAdd;
		if (data->frame > data->config.buffer->frames) {
			data->frame -= data->config.buffer->frames;
		}
	}
	if (data->header.pNext) {
		return azaProcessDSP(buffer, data->header.pNext);
	}
	return AZA_SUCCESS;
}



uint32_t azaGateGetAllocSize() {
	size_t size = sizeof(azaGate);
	size = azaAddSizeWithAlign(size, azaRMSGetAllocSize((azaRMSConfig) { 128 }, 1), alignof(azaRMS));
	return (uint32_t)size;
}

void azaGateInit(azaGate *data, uint32_t allocSize, azaGateConfig config) {
	data->header.kind = AZA_DSP_GATE;
	data->header.structSize = allocSize;
	data->config = config;
	azaRMSConfig rmsConfig = (azaRMSConfig) { 128 };
	azaRMSInit(&data->rms, azaRMSGetAllocSize(rmsConfig, 1), rmsConfig, 1);
}

void azaGateDeinit(azaGate *data) {
	azaRMSDeinit(&data->rms);
}

azaGate* azaMakeGate(azaGateConfig config) {
	uint32_t size = azaGateGetAllocSize();
	azaGate *result = aza_calloc(1, size);
	azaGateInit(result, size, config);
	return result;
}

void azaFreeGate(azaGate *data) {
	azaGateDeinit(data);
	aza_free(data);
}

int azaProcessGate(azaBuffer buffer, azaGate *data) {
	azaBuffer rmsBuffer = azaPushSideBuffer(buffer.frames, 1, buffer.samplerate);
	azaBuffer activationBuffer = buffer;
	uint8_t sideBuffersInUse = 1;

	if (data->config.activationEffects) {
		activationBuffer = azaPushSideBufferCopy(buffer);
		sideBuffersInUse++;
		int err = azaProcessDSP(activationBuffer, data->config.activationEffects);
		if (err) {
			azaPopSideBuffers(sideBuffersInUse);
			return err;
		}
	}

	azaProcessRMSCombined(rmsBuffer, activationBuffer, &data->rms, azaOpMax);
	float t = (float)buffer.samplerate / 1000.0f;
	float attackFactor = expf(-1.0f / (data->config.attack * t));
	float decayFactor = expf(-1.0f / (data->config.decay * t));

	for (size_t i = 0; i < buffer.frames; i++) {
		float rms = aza_amp_to_dbf(rmsBuffer.samples[i]);
		if (rms < -120.0f) rms = -120.0f;
		if (rms > data->config.threshold) {
			data->attenuation = rms + attackFactor * (data->attenuation - rms);
		} else {
			data->attenuation = rms + decayFactor * (data->attenuation - rms);
		}
		float gain;
		if (data->attenuation > data->config.threshold) {
			gain = 0.0f;
		} else {
			gain = -10.0f * (data->config.threshold - data->attenuation);
		}
		data->gain = gain;
		float amp = aza_db_to_ampf(gain);
		for (uint8_t c = 0; c < buffer.channels.count; c++) {
			buffer.samples[i * buffer.stride + c] *= amp;
		}
	}
	azaPopSideBuffers(sideBuffersInUse);
	if (data->header.pNext) {
		return azaProcessDSP(buffer, data->header.pNext);
	}
	return AZA_SUCCESS;
}



azaDelayDynamicChannelConfig* azaDelayDynamicGetChannelConfig(azaDelayDynamic *data, uint8_t channel) {
	azaDelayDynamicChannelData *channelData = azaGetChannelData(&data->channelData, channel);
	return &channelData->config;
}

static azaKernel* azaDelayDynamicGetKernel(azaDelayDynamic *data) {
	azaKernel *kernel = data->config.kernel;
	if (!kernel) {
		kernel = &azaKernelDefaultLanczos;
	}
	return kernel;
}

// Handles resizing of buffers if needed
static void azaDelayDynamicHandleBufferResizes(azaDelayDynamic *data, azaBuffer src) {
	// TODO: Probably track channel layouts and handle them changing. Right now the buffers will break if the number of channels changes.
	azaEnsureChannels(&data->channelData, src.channels.count);
	uint32_t kernelSamples;
	azaKernel *kernel = azaDelayDynamicGetKernel(data);
	kernelSamples = (uint32_t)ceilf(kernel->isSymmetrical ? (kernel->length - 1.0f) * 2.0f : (kernel->length-1.0f));
	// kernelSamples = 0;
	uint32_t delaySamplesMax = (uint32_t)ceilf(aza_ms_to_samples(data->config.delayMax, (float)src.samplerate)) + kernelSamples;
	uint32_t totalSamplesNeeded = delaySamplesMax + src.frames;
	uint32_t perChannelBufferCap = data->bufferCap / src.channels.count;
	if (perChannelBufferCap >= totalSamplesNeeded) return;
	// Have to realloc buffer
	uint32_t newPerChannelBufferCap = (uint32_t)aza_grow(perChannelBufferCap, totalSamplesNeeded, 256);
	float *newBuffer = aza_calloc(sizeof(float), newPerChannelBufferCap * src.channels.count);
	for (uint8_t c = 0; c < src.channels.count; c++) {
		azaDelayDynamicChannelData *channelData = azaGetChannelData(&data->channelData, c);
		float *newChannelBuffer = newBuffer + c * newPerChannelBufferCap;
		if (data->buffer) {
			memcpy(newChannelBuffer + newPerChannelBufferCap - perChannelBufferCap, channelData->buffer, sizeof(float) * perChannelBufferCap);
		}
		channelData->buffer = newChannelBuffer;
		// Maybe we don't have to do this because we probably do it in Process
		// channelData->delaySamples = aza_ms_to_samples(channelData->config.delay, (float)samplerate);
	}
	if (data->buffer) {
		aza_free(data->buffer);
	}
	data->buffer = newBuffer;
	data->bufferCap = newPerChannelBufferCap * src.channels.count;
}

// Puts new audio data into the buffer for immediate sampling. Assumes azaDelayDynamicHandleBufferResizes was called already.
static void azaDelayDynamicPrimeBuffer(azaDelayDynamic *data, azaBuffer src) {
	uint32_t kernelSamples;
	azaKernel *kernel = azaDelayDynamicGetKernel(data);
	kernelSamples = (uint32_t)ceilf(kernel->isSymmetrical ? (kernel->length - 1.0f) * 2.0f : (kernel->length-1.0f));
	// kernelSamples = 0;
	uint32_t delaySamplesMax = (uint32_t)ceilf(aza_ms_to_samples(data->config.delayMax, (float)src.samplerate)) + kernelSamples;
	for (uint8_t c = 0; c < src.channels.count; c++) {
		azaDelayDynamicChannelData *channelData = azaGetChannelData(&data->channelData, c);
		// Move existing buffer back to make room for new buffer data
		// This should work because we're expecting each buffer to be at least delaySamplesMax+src.frames in size
		for (uint32_t i = 0; i < delaySamplesMax; i++) {
			channelData->buffer[i] = channelData->buffer[i+src.frames];
		}
		azaBufferCopyChannel((azaBuffer) {
			.samples = channelData->buffer + delaySamplesMax,
			.samplerate = src.samplerate,
			.frames = src.frames,
			.stride = 1,
			.channels = (azaChannelLayout) { .count = 1 },
		}, 0, src, c);
	}
}

uint32_t azaDelayDynamicGetAllocSize(uint8_t channelCapInline) {
	size_t size = sizeof(azaDelayDynamic);
	size = azaAddSizeWithAlign(size, channelCapInline * sizeof(azaDelayDynamicChannelData), alignof(azaDelayDynamicChannelData));
	return (uint32_t)size;
}

void azaDelayDynamicInit(azaDelayDynamic *data, uint32_t allocSize, azaDelayDynamicConfig config, uint8_t channelCapInline, uint8_t channelCount, azaDelayDynamicChannelConfig *channelConfigs) {
	data->header.kind = AZA_DSP_DELAY_DYNAMIC;
	data->header.structSize = allocSize;
	data->config = config;
	azaDSPChannelDataInit(&data->channelData, channelCapInline, sizeof(azaDelayDynamicChannelData), alignof(azaDelayDynamicChannelData));
	azaEnsureChannels(&data->channelData, channelCount);
	if (channelConfigs) {
		for (uint8_t c = 0; c < channelCount; c++) {
			azaDelayDynamicChannelConfig *channelConfig = azaDelayDynamicGetChannelConfig(data, c);
			*channelConfig = channelConfigs[c];
		}
	}
}

void azaDelayDynamicDeinit(azaDelayDynamic *data) {
	if (data->buffer) {
		aza_free(data->buffer);
	}
	data->buffer = NULL;
	data->bufferCap = 0;
}

azaDelayDynamic* azaMakeDelayDynamic(azaDelayDynamicConfig config, uint8_t channelCapInline, uint8_t channelCount, azaDelayDynamicChannelConfig *channelConfigs) {
	uint32_t size = azaDelayDynamicGetAllocSize(channelCapInline);
	azaDelayDynamic *result = aza_calloc(1, size);
	azaDelayDynamicInit(result, size, config, channelCapInline, channelCount, channelConfigs);
	return result;
}

void azaFreeDelayDynamic(azaDelayDynamic *data) {
	azaDelayDynamicDeinit(data);
	aza_free(data);
}

int azaProcessDelayDynamic(azaBuffer buffer, azaDelayDynamic *data, float *endChannelDelays) {
	int err = AZA_SUCCESS;
	uint8_t numSideBuffers = 0;
	if (data == NULL) {
		err = AZA_ERROR_NULL_POINTER;
		goto error;
	} else {
		err = azaCheckBuffer(buffer);
		if (err) goto error;
	}
	azaKernel *kernel = azaDelayDynamicGetKernel(data);
	azaBuffer inputBuffer;
	if (data->config.wetEffects) {
		inputBuffer = azaPushSideBufferCopy(buffer);
		numSideBuffers++;
		err = azaProcessDSP(inputBuffer, data->config.wetEffects);
		if (err) goto error;
	} else {
		inputBuffer = buffer;
	}
	azaDelayDynamicHandleBufferResizes(data, inputBuffer);
	// TODO: Verify whether this matters. It was difficult to tell whether there was any problem with not factoring in the kernel (which I suppose would only matter for very close to 0 delay).
	int kernelSamplesLeft, kernelSamplesRight;
	if (kernel->isSymmetrical) {
		kernelSamplesLeft = (int)ceilf(kernel->length - 1.0f);
		kernelSamplesRight = (int)ceilf(kernel->length - 1.0f);
	} else {
		kernelSamplesLeft = 0;
		kernelSamplesRight = (int)ceilf(kernel->length - 1.0f);
	}
	// kernelSamplesLeft = 0;
	// kernelSamplesRight = 0;
	uint32_t delaySamplesMax = (uint32_t)ceilf(aza_ms_to_samples(data->config.delayMax, (float)buffer.samplerate));
	for (uint8_t c = 0; c < inputBuffer.channels.count; c++) {
		azaDelayDynamicChannelData *channelData = azaGetChannelData(&data->channelData, c);
		float startIndex = (float)delaySamplesMax - aza_ms_to_samples(channelData->config.delay, (float)inputBuffer.samplerate);
		float endIndex = startIndex + (float)inputBuffer.frames;
		if (endChannelDelays) {
			endIndex -= aza_ms_to_samples(endChannelDelays[c] - channelData->config.delay, (float)inputBuffer.samplerate);
		}
		startIndex = azaClampf(startIndex, 0.0f, (float)delaySamplesMax);
		endIndex = azaClampf(endIndex, 0.0f, (float)delaySamplesMax);
		if (startIndex >= endIndex) continue;
		uint8_t c2 = (c + 1) % inputBuffer.channels.count;
		for (uint32_t i = 0; i < inputBuffer.frames; i++) {
			float index = lerp(startIndex, endIndex, (float)i / (float)inputBuffer.frames);
			uint32_t s = i * inputBuffer.stride + c;
			float toAdd = inputBuffer.samples[s];
			if (data->config.feedback != 0.0f) {
			 	toAdd += azaSampleWithKernel(channelData->buffer+kernelSamplesLeft, 1, -kernelSamplesLeft, delaySamplesMax+kernelSamplesRight+inputBuffer.frames, kernel, index) * data->config.feedback;
			}
			inputBuffer.samples[i * inputBuffer.stride + c] += toAdd * (1.0f - data->config.pingpong);
			inputBuffer.samples[i * inputBuffer.stride + c2] += toAdd * data->config.pingpong;
		}
	}
	azaDelayDynamicPrimeBuffer(data, inputBuffer);
	for (uint8_t c = 0; c < buffer.channels.count; c++) {
		azaDelayDynamicChannelData *channelData = azaGetChannelData(&data->channelData, c);
		float startIndex = (float)delaySamplesMax - aza_ms_to_samples(channelData->config.delay, (float)buffer.samplerate);
		float endIndex = startIndex + (float)buffer.frames;
		if (endChannelDelays) {
			endIndex -= aza_ms_to_samples(endChannelDelays[c] - channelData->config.delay, (float)buffer.samplerate);
			channelData->config.delay = endChannelDelays[c];
		}
		startIndex = azaClampf(startIndex, 0.0f, (float)(delaySamplesMax + buffer.frames));
		endIndex = azaClampf(endIndex, 0.0f, (float)(delaySamplesMax + buffer.frames));
		float amount = aza_db_to_ampf(data->config.gain);
		float amountDry = aza_db_to_ampf(data->config.gainDry);
		if (startIndex >= endIndex) amount = 0.0f;
		for (uint32_t i = 0; i < buffer.frames; i++) {
			float index = lerp(startIndex, endIndex, (float)i / (float)buffer.frames);
			uint32_t s = i * buffer.stride + c;
			float wet = azaSampleWithKernel(channelData->buffer+kernelSamplesLeft, 1, -kernelSamplesLeft, delaySamplesMax+kernelSamplesRight+buffer.frames, kernel, index);
			buffer.samples[s] = wet * amount + buffer.samples[s] * amountDry;
		}
	}
	if (data->header.pNext) {
		return azaProcessDSP(buffer, data->header.pNext);
	}
error:
	azaPopSideBuffers(numSideBuffers);
	return err;
}



void azaKernelInit(azaKernel *kernel, int isSymmetrical, float length, float scale) {
	assert(length > 0.0f);
	assert(scale > 0.0f);
	kernel->isSymmetrical = isSymmetrical;
	kernel->length = length;
	kernel->scale = scale;
	kernel->size = (uint32_t)ceilf(length * scale);
	kernel->table = aza_calloc(kernel->size, sizeof(float));
}

void azaKernelDeinit(azaKernel *kernel) {
	aza_free(kernel->table);
}

float azaKernelSample(azaKernel *kernel, float x) {
	if (kernel->isSymmetrical) {
		if (x < 0.0f) x = -x;
	} else {
		if (x < 0.0f) return 0.0f;
	}
	x *= kernel->scale;
	uint32_t index = (uint32_t)x;
	if (index >= kernel->size-1) return 0.0f;
	x -= (float)index;
	return lerp(kernel->table[index], kernel->table[index+1], x);
}

void azaKernelMakeLanczos(azaKernel *kernel, float resolution, float radius) {
	azaKernelInit(kernel, 1, 1+radius, resolution);
	for (uint32_t i = 0; i < kernel->size-1; i++) {
		kernel->table[i] = lanczos((float)i / resolution, radius);
	}
	kernel->table[kernel->size-1] = 0.0f;
}

float azaSampleWithKernel(float *src, int stride, int minFrame, int maxFrame, azaKernel *kernel, float pos) {
	float result = 0.0f;
	int start, end;
	if (kernel->isSymmetrical) {
		start = (int)pos - (int)kernel->length + 1;
		end = (int)pos + (int)kernel->length;
	} else {
		start = (int)pos;
		end = (int)pos + (int)kernel->length;
	}
	for (int i = start; i < end; i++) {
		int index = AZA_CLAMP(i, minFrame, maxFrame-1);
		float s = src[index * stride];
		result += s * azaKernelSample(kernel, (float)i - pos);
	}
	return result;
}

void azaResample(azaKernel *kernel, float factor, float *dst, int dstStride, int dstFrames, float *src, int srcStride, int srcFrameMin, int srcFrameMax, float srcSampleOffset) {
	for (uint32_t i = 0; i < (uint32_t)dstFrames; i++) {
		float pos = (float)i * factor + srcSampleOffset;
		dst[i * dstStride] = azaSampleWithKernel(src, srcStride, srcFrameMin, srcFrameMax, kernel, pos);
	}
}

void azaResampleAdd(azaKernel *kernel, float factor, float amp, float *dst, int dstStride, int dstFrames, float *src, int srcStride, int srcFrameMin, int srcFrameMax, float srcSampleOffset) {
	for (uint32_t i = 0; i < (uint32_t)dstFrames; i++) {
		float pos = (float)i * factor + srcSampleOffset;
		dst[i * dstStride] += amp * azaSampleWithKernel(src, srcStride, srcFrameMin, srcFrameMax, kernel, pos);
	}
}

#define PRINT_CHANNEL_AMPS 0
#define PRINT_CHANNEL_DELAYS 0

#if PRINT_CHANNEL_AMPS || PRINT_CHANNEL_DELAYS
static int repeatCount = 0;
#endif

struct channelMetadata {
	float amp;
	uint32_t channel;
};

int compareChannelMetadataAmp(const void *_lhs, const void *_rhs) {
	const struct channelMetadata *lhs = _lhs;
	const struct channelMetadata *rhs = _rhs;
	if (lhs->amp == rhs->amp) return 0;
	// We want descending order
	if (lhs->amp < rhs->amp) return 1;
	return -1;
};
int compareChannelMetadataChannel(const void *_lhs, const void *_rhs) {
	const struct channelMetadata *lhs = _lhs;
	const struct channelMetadata *rhs = _rhs;
	if (lhs->channel == rhs->channel) return 0;
	// We want ascending order
	if (lhs->channel < rhs->channel) return -1;
	return 1;
};


static void azaGatherChannelPresenseMetadata(azaChannelLayout channelLayout, uint8_t *hasFront, uint8_t *hasMidFront, uint8_t *hasSub, uint8_t *hasBack, uint8_t *hasSide, uint8_t *hasAerials, uint8_t *subChannel) {
	for (uint8_t i = 0; i < channelLayout.count; i++) {
		switch (channelLayout.positions[i]) {
			case AZA_POS_LEFT_FRONT:
			case AZA_POS_CENTER_FRONT:
			case AZA_POS_RIGHT_FRONT:
				*hasFront = 1;
				break;
			case AZA_POS_LEFT_CENTER_FRONT:
			case AZA_POS_RIGHT_CENTER_FRONT:
				*hasMidFront = 1;
				break;
			case AZA_POS_SUBWOOFER:
				*hasSub = 1;
				*subChannel = i;
				break;
			case AZA_POS_LEFT_BACK:
			case AZA_POS_CENTER_BACK:
			case AZA_POS_RIGHT_BACK:
				*hasBack = 1;
				break;
			case AZA_POS_LEFT_SIDE:
			case AZA_POS_RIGHT_SIDE:
				*hasSide = 1;
				break;
			case AZA_POS_CENTER_TOP:
				*hasAerials = 1;
				break;
			case AZA_POS_LEFT_FRONT_TOP:
			case AZA_POS_CENTER_FRONT_TOP:
			case AZA_POS_RIGHT_FRONT_TOP:
				*hasFront = 1;
				*hasAerials = 1;
				break;
			case AZA_POS_LEFT_BACK_TOP:
			case AZA_POS_CENTER_BACK_TOP:
			case AZA_POS_RIGHT_BACK_TOP:
				*hasBack = 1;
				*hasAerials = 1;
				break;
		}
	}
}

static void azaGetChannelMetadata(azaChannelLayout channelLayout, azaVec3 *dstVectors, uint8_t *nonSubChannels, uint8_t *hasAerials) {
	uint8_t hasFront = 0, hasMidFront = 0, hasSub = 0, hasBack = 0, hasSide = 0, subChannel = 0;
	*hasAerials = 0;
	azaGatherChannelPresenseMetadata(channelLayout, &hasFront, &hasMidFront, &hasSub, &hasBack, &hasSide, hasAerials, &subChannel);
	*nonSubChannels = hasSub ? channelLayout.count-1 : channelLayout.count;
	// Angles are relative to front center, to be signed later
	// These relate to anglePhi above
	float angleFront = AZA_DEG_TO_RAD(75.0f), angleMidFront = AZA_DEG_TO_RAD(30.0f), angleSide = AZA_DEG_TO_RAD(90.0f), angleBack = AZA_DEG_TO_RAD(130.0f);
	if (hasFront && hasMidFront && hasSide && hasBack) {
		// Standard 8 or 9 speaker layout
		angleFront = AZA_DEG_TO_RAD(60.0f);
		angleMidFront = AZA_DEG_TO_RAD(30.0f);
		angleBack = AZA_DEG_TO_RAD(140.0f);
	} else if (hasFront && hasSide && hasBack) {
		// Standard 6 or 7 speaker layout
		angleFront = AZA_DEG_TO_RAD(60.0f);
		angleBack = AZA_DEG_TO_RAD(140.0f);
	} else if (hasFront && hasBack) {
		// Standard 4 or 5 speaker layout
		angleFront = AZA_DEG_TO_RAD(60.0f);
		angleBack = AZA_DEG_TO_RAD(115.0f);
	} else if (hasFront) {
		// Standard 2 or 3 speaker layout
		angleFront = AZA_DEG_TO_RAD(75.0f);
	} else if (hasBack) {
		// Weird, will probably never actually happen, but we can work with it
		angleBack = AZA_DEG_TO_RAD(110.0f);
	} else {
		// We're confused, just do anything
		angleFront = AZA_DEG_TO_RAD(45.0f);
		angleMidFront = AZA_DEG_TO_RAD(22.5f);
		angleSide = AZA_DEG_TO_RAD(90.0f);
		angleBack = AZA_DEG_TO_RAD(120.0f);
	}
	for (uint8_t i = 0; i < channelLayout.count; i++) {
		switch (channelLayout.positions[i]) {
			case AZA_POS_LEFT_FRONT:
				dstVectors[i] = (azaVec3) { sinf(-angleFront), 0.0f, cosf(-angleFront) };
				break;
			case AZA_POS_CENTER_FRONT:
				dstVectors[i] = (azaVec3) { 0.0f, 0.0f, 1.0f };
				break;
			case AZA_POS_RIGHT_FRONT:
				dstVectors[i] = (azaVec3) { sinf(angleFront), 0.0f, cosf(angleFront) };
				break;
			case AZA_POS_LEFT_CENTER_FRONT:
				dstVectors[i] = (azaVec3) { sinf(-angleMidFront), 0.0f, cosf(-angleMidFront) };
				break;
			case AZA_POS_RIGHT_CENTER_FRONT:
				dstVectors[i] = (azaVec3) { sinf(angleMidFront), 0.0f, cosf(angleMidFront) };
				break;
			case AZA_POS_LEFT_BACK:
				dstVectors[i] = (azaVec3) { sinf(-angleBack), 0.0f, cosf(-angleBack) };
				break;
			case AZA_POS_CENTER_BACK:
				dstVectors[i] = (azaVec3) { 0.0f, 0.0f, -1.0f };
				break;
			case AZA_POS_RIGHT_BACK:
				dstVectors[i] = (azaVec3) { sinf(angleBack), 0.0f, cosf(angleBack) };
				break;
			case AZA_POS_LEFT_SIDE:
				dstVectors[i] = (azaVec3) { sinf(-angleSide), 0.0f, cosf(-angleSide) };
				break;
			case AZA_POS_RIGHT_SIDE:
				dstVectors[i] = (azaVec3) { sinf(angleSide), 0.0f, cosf(angleSide) };
				break;
			case AZA_POS_CENTER_TOP:
				dstVectors[i] = (azaVec3) { 0.0f, 1.0f, 0.0f };
				break;
			case AZA_POS_LEFT_FRONT_TOP:
				dstVectors[i] = azaVec3Normalized((azaVec3) { sinf(-angleFront), 1.0f, cosf(-angleFront) });
				break;
			case AZA_POS_CENTER_FRONT_TOP:
				dstVectors[i] = azaVec3Normalized((azaVec3) { 0.0f, 1.0f, 1.0f });
				break;
			case AZA_POS_RIGHT_FRONT_TOP:
				dstVectors[i] = azaVec3Normalized((azaVec3) { sinf(angleFront), 1.0f, cosf(angleFront) });
				break;
			case AZA_POS_LEFT_BACK_TOP:
				dstVectors[i] = azaVec3Normalized((azaVec3) { sinf(-angleBack), 1.0f, cosf(-angleBack) });
				break;
			case AZA_POS_CENTER_BACK_TOP:
				dstVectors[i] = azaVec3Normalized((azaVec3) { 0.0f, 1.0f, -1.0f });
				break;
			case AZA_POS_RIGHT_BACK_TOP:
				dstVectors[i] = azaVec3Normalized((azaVec3) { sinf(angleBack), 1.0f, cosf(angleBack) });
				break;
			default: // This includes AZA_POS_SUBWOOFER
				continue;
		}
	}
}

static uint32_t azaSpatializeChannelDataGetAllocSize(uint8_t channelCapInline) {
	return channelCapInline * azaFilterGetAllocSize(1);
}

uint32_t azaSpatializeGetAllocSize(uint8_t channelCapInline) {
	size_t size = sizeof(azaSpatialize);
	size = azaAddSizeWithAlign(size, azaSpatializeChannelDataGetAllocSize(channelCapInline), alignof(azaSpatializeChannelData));
	size = azaAddSizeWithAlign(size, azaDelayDynamicGetAllocSize(channelCapInline), alignof(azaDelayDynamic));
	return (uint32_t)size;
}

void azaSpatializeInit(azaSpatialize *data, uint32_t allocSize, azaSpatializeConfig config, uint8_t channelCapInline) {
	uint32_t filterAllocSize = azaFilterGetAllocSize(1);
	data->header.kind = AZA_DSP_SPATIALIZE;
	data->header.structSize = allocSize;
	data->config = config;
	azaDSPChannelDataInit(&data->channelData, channelCapInline, filterAllocSize, alignof(azaSpatializeChannelData));
	for (uint8_t c = 0; c < channelCapInline; c++) {
		azaSpatializeChannelData *channelData = azaGetChannelData(&data->channelData, c);
		azaFilterInit(&channelData->filter, filterAllocSize, (azaFilterConfig) {
			.kind = AZA_FILTER_LOW_PASS,
			.dryMix = 0.0f,
			.frequency = 15000.0f,
		}, 1);
	}
	azaDelayDynamic *delay = azaSpatializeGetDelayDynamic(data);
	azaDelayDynamicInit(delay, azaDelayDynamicGetAllocSize(channelCapInline), (azaDelayDynamicConfig){
		.gain = 0.0f,
		.gainDry = -INFINITY,
		.delayMax = config.delayMax != 0.0f ? config.delayMax : 500.0f,
		.feedback = 0.0f,
		.pingpong = 0.0f,
		.wetEffects = NULL,
		.kernel = NULL,
	}, channelCapInline, channelCapInline, NULL);
}

void azaSpatializeDeinit(azaSpatialize *data) {
	for (uint8_t c = 0; c < data->channelData.capInline + data->channelData.capAdditional; c++) {
		azaSpatializeChannelData *channelData = azaGetChannelData(&data->channelData, c);
		azaFilterDeinit(&channelData->filter);
	}
	azaDelayDynamic *delay = azaSpatializeGetDelayDynamic(data);
	azaDelayDynamicDeinit(delay);
}

azaDelayDynamic* azaSpatializeGetDelayDynamic(azaSpatialize *data) {
	azaDelayDynamic *result = (azaDelayDynamic*)azaGetBufferOffset((char*)data, sizeof(azaSpatialize) + data->channelData.size * data->channelData.capInline, alignof(azaDelayDynamic));
	return result;
}

azaSpatialize* azaMakeSpatialize(azaSpatializeConfig config, uint8_t channelCapInline) {
	uint32_t size = azaSpatializeGetAllocSize(channelCapInline);
	azaSpatialize *result = aza_calloc(1, size);
	azaSpatializeInit(result, size, config, channelCapInline);
	return result;
}

void azaFreeSpatialize(azaSpatialize *data) {
	azaSpatializeDeinit(data);
	aza_free(data);
}

static float azaSpatializeGetFilterCutoff(float delay, float dot) {
	return 192000.0f / AZA_MAX(delay, 1.0f) * (dot * 0.35f + 0.65f);
}

int azaProcessSpatialize(azaSpatialize *data, azaBuffer dstBuffer, azaBuffer srcBuffer, azaVec3 srcPosStart, float srcAmpStart, azaVec3 srcPosEnd, float srcAmpEnd) {
	int err = AZA_SUCCESS;
	assert(dstBuffer.channels.count <= AZA_MAX_CHANNEL_POSITIONS);
	assert(dstBuffer.samplerate == srcBuffer.samplerate);
	assert(dstBuffer.frames == srcBuffer.frames);
	assert(srcBuffer.channels.count == 1);
	if (dstBuffer.channels.count == 0) {
		// What are we even doing
		return AZA_SUCCESS;
	}
	const azaWorld *world = data->config.world;
	if (world == NULL) {
		world = &azaWorldDefault;
	}
	// Transform srcPos to headspace
	srcPosStart = azaMulVec3Mat3(azaSubVec3(srcPosStart, world->origin), world->orientation);
	srcPosEnd = azaMulVec3Mat3(azaSubVec3(srcPosEnd, world->origin), world->orientation);
	azaVec3 srcNormalStart;
	azaVec3 srcNormalEnd;

	azaBuffer sideBuffer = azaPushSideBufferZero(dstBuffer.frames, dstBuffer.channels.count, dstBuffer.samplerate);
	float delayStart = azaVec3Norm(srcPosStart) / world->speedOfSound * 1000.0f;
	float delayEnd = azaVec3Norm(srcPosEnd) / world->speedOfSound * 1000.0f;
	if (dstBuffer.channels.count == 1) {
		// Nothing to do but put it in there I guess
		if (data->config.mode == AZA_SPATIALIZE_ADVANCED) {
			// Gotta do the doppler
			azaDelayDynamic *delay = azaSpatializeGetDelayDynamic(data);
			azaEnsureChannels(&data->channelData, 1);
			azaSpatializeChannelData *channelData = azaGetChannelData(&data->channelData, 0);
			azaEnsureChannels(&delay->channelData, 1);
			azaDelayDynamicChannelConfig *channelConfig = azaDelayDynamicGetChannelConfig(delay, 0);
			channelConfig->delay = delayStart;
			azaProcessDelayDynamic(sideBuffer, delay, &delayEnd);
			channelData->filter.config.frequency = azaSpatializeGetFilterCutoff(delayStart, 1.0f);
			azaProcessFilter(sideBuffer, &channelData->filter);
		}
		azaBufferMixFade(dstBuffer, 1.0f, 1.0f, sideBuffer, srcAmpStart, srcAmpEnd);
		azaPopSideBuffer();
		return AZA_SUCCESS;
	}
	// How much of the signal to add to all channels in case srcPos is crossing close to the head
	float allChannelAddAmpStart = 0.0f;
	float allChannelAddAmpEnd = 0.0f;
	float normStart, normEnd;
	{
		normStart = azaVec3Norm(srcPosStart);
		if (normStart < 0.5f) {
			allChannelAddAmpStart = (0.5f - normStart) * 2.0f;
			srcNormalStart = srcPosStart;
		} else {
			srcNormalStart = azaDivVec3Scalar(srcPosStart, normStart);
		}
		normEnd = azaVec3Norm(srcPosEnd);
		if (normEnd < 0.5f) {
			allChannelAddAmpEnd = (0.5f - normEnd) * 2.0f;
			srcNormalEnd = srcPosEnd;
		} else {
			srcNormalEnd = azaDivVec3Scalar(srcPosEnd, normEnd);
		}
	}

	uint8_t nonSubChannels, hasAerials;
	azaVec3 channelVectors[AZA_MAX_CHANNEL_POSITIONS];
	azaGetChannelMetadata(dstBuffer.channels, channelVectors, &nonSubChannels, &hasAerials);
	float channelDelayStart[AZA_MAX_CHANNEL_POSITIONS];
	float channelDelayEnd[AZA_MAX_CHANNEL_POSITIONS];
	float channelDot[AZA_MAX_CHANNEL_POSITIONS];

	// Position our channel vectors
	struct channelMetadata channelsStart[AZA_MAX_CHANNEL_POSITIONS];
	struct channelMetadata channelsEnd[AZA_MAX_CHANNEL_POSITIONS];
	memset(channelsStart, 0, sizeof(channelsStart));
	memset(channelsEnd, 0, sizeof(channelsEnd));
	float totalMagnitudeStart = 0.0f;
	float totalMagnitudeEnd = 0.0f;
	float earDistance = data->config.earDistance;
	if (earDistance == 0.0f) {
		earDistance = 0.085f;
	}
	for (uint8_t i = 0; i < dstBuffer.channels.count; i++) {
		channelsStart[i].channel = i;
		channelsEnd[i].channel = i;
		channelDot[i] = azaVec3Dot(channelVectors[i], srcNormalStart);
		channelsStart[i].amp = 0.5f * normStart + 0.5f * channelDot[i] + allChannelAddAmpStart / (float)nonSubChannels;
		channelsEnd[i].amp = 0.5f * normEnd + 0.5f * azaVec3Dot(channelVectors[i], srcNormalEnd) + allChannelAddAmpEnd / (float)nonSubChannels;
		azaVec3 earPos = azaMulVec3Scalar(channelVectors[i], earDistance);
		if (data->config.mode == AZA_SPATIALIZE_ADVANCED) {
			// channelDelayStart[i] = 0.01f;
			// channelDelayEnd[i] = 0.01f;
			channelDelayStart[i] = azaVec3Norm(azaSubVec3(srcPosStart, earPos)) / world->speedOfSound * 1000.0f;
			channelDelayEnd[i] = azaVec3Norm(azaSubVec3(srcPosEnd, earPos)) / world->speedOfSound * 1000.0f;
		} else {
			channelDelayStart[i] = delayStart;
			channelDelayEnd[i] = delayEnd;
		}
		// channelsStart[i].amp = azaVec3Dot(channelVectors[i], srcNormalStart) + allChannelAddAmpStart / (float)nonSubChannels;
		// channelsEnd[i].amp = azaVec3Dot(channelVectors[i], srcNormalEnd) + allChannelAddAmpEnd / (float)nonSubChannels;
		// if (channelsStart[i].amp < 0.0f) channelsStart[i].amp = 0.0f;
		// if (channelsEnd[i].amp < 0.0f) channelsEnd[i].amp = 0.0f;
		// channelsStart[i].amp = 0.25f + 0.75f * channelsStart[i].amp;
		// channelsEnd[i].amp = 0.25f + 0.75f * channelsEnd[i].amp;
		totalMagnitudeStart += channelsStart[i].amp;
		totalMagnitudeEnd += channelsEnd[i].amp;
	}

	float minAmp = data->config.mode == AZA_SPATIALIZE_SIMPLE ? 0.0f : 0.8f;

	if (dstBuffer.channels.count > 2) {
		int minChannel = 2;
		if (dstBuffer.channels.count > 3 && hasAerials) {
			// TODO: This probably isn't a reliable way to use aerials. Probably do something smarter.
			minChannel = 3;
		}
		// Get channel amps in descending order
		qsort(channelsStart, dstBuffer.channels.count, sizeof(struct channelMetadata), compareChannelMetadataAmp);
		qsort(channelsEnd, dstBuffer.channels.count, sizeof(struct channelMetadata), compareChannelMetadataAmp);

		float ampMaxRangeStart = channelsStart[0].amp;
		float ampMaxRangeEnd = channelsEnd[0].amp;
		float ampMinRangeStart = channelsStart[minChannel].amp;
		float ampMinRangeEnd = channelsEnd[minChannel].amp;
		totalMagnitudeStart = 0.0f;
		totalMagnitudeEnd = 0.0f;
		for (uint8_t i = 0; i < dstBuffer.channels.count; i++) {
			channelsStart[i].amp = linstepf(channelsStart[i].amp, ampMinRangeStart, ampMaxRangeStart) + allChannelAddAmpStart / (float)nonSubChannels;
			channelsEnd[i].amp = linstepf(channelsEnd[i].amp, ampMinRangeEnd, ampMaxRangeEnd) + allChannelAddAmpEnd / (float)nonSubChannels;
			totalMagnitudeStart += channelsStart[i].amp;
			totalMagnitudeEnd += channelsEnd[i].amp;
		}

		// Put the amps back into channel order
		qsort(channelsStart, dstBuffer.channels.count, sizeof(struct channelMetadata), compareChannelMetadataChannel);
		qsort(channelsEnd, dstBuffer.channels.count, sizeof(struct channelMetadata), compareChannelMetadataChannel);
	}

#if PRINT_CHANNEL_AMPS || PRINT_CHANNEL_DELAYS
	if (repeatCount == 0) {
		AZA_LOG_INFO("\n");
	}
#endif
	for (uint8_t c = 0; c < sideBuffer.channels.count; c++) {
		float ampStart = srcAmpStart;
		float ampEnd = srcAmpEnd;
		if (dstBuffer.channels.positions[c] != AZA_POS_SUBWOOFER) {
			ampStart *= (channelsStart[c].amp / totalMagnitudeStart) * (1.0f - minAmp) + minAmp;
			ampEnd *= (channelsEnd[c].amp / totalMagnitudeEnd) * (1.0f - minAmp) + minAmp;
		}
#if PRINT_CHANNEL_AMPS
		if (repeatCount == 0) {
			AZA_LOG_INFO("Channel %u amp: %f\n", (uint32_t)c, ampStart);
		}
#endif
#if PRINT_CHANNEL_DELAYS
		if (repeatCount == 0) {
			AZA_LOG_INFO("Channel %u delay: %f\n", (uint32_t)c, channelDelayStart[c]);
		}
#endif
		azaBufferMixFade(azaBufferOneChannel(sideBuffer, c), 1.0f, 1.0f, srcBuffer, ampStart, ampEnd);
	}
	if (data->config.mode == AZA_SPATIALIZE_ADVANCED) {
		// Gotta do the doppler
		azaDelayDynamic *delay = azaSpatializeGetDelayDynamic(data);
		azaEnsureChannels(&data->channelData, sideBuffer.channels.count);
		azaEnsureChannels(&delay->channelData, sideBuffer.channels.count);
		for (uint8_t c = 0; c < sideBuffer.channels.count; c++) {
			azaDelayDynamicChannelConfig *channelConfig = azaDelayDynamicGetChannelConfig(delay, c);
			channelConfig->delay = channelDelayStart[c];
			azaSpatializeChannelData *channelData = azaGetChannelData(&data->channelData, c);
			channelData->filter.config.frequency = azaSpatializeGetFilterCutoff(channelDelayStart[c], channelDot[c]);
			// AZA_LOG_INFO("(c %u) filter freq = %f\n", c, channelData->filter.config.frequency);
			azaProcessFilter(azaBufferOneChannel(sideBuffer, c), &channelData->filter);
		}
		azaProcessDelayDynamic(sideBuffer, delay, channelDelayEnd);
	}
	azaBufferMix(dstBuffer, 1.0f, sideBuffer, 1.0f);
#if PRINT_CHANNEL_AMPS || PRINT_CHANNEL_DELAYS
	repeatCount = (repeatCount + 1) % 10;
#endif
	azaPopSideBuffer();
	return err;
}
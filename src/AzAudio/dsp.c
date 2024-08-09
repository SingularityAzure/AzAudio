/*
	File: dsp.c
	Author: Philip Haynes
*/

#include "dsp.h"

#include "error.h"
#include "helpers.h"

#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <assert.h>


#define AZA_MAX_SIDE_BUFFERS 64
thread_local azaBuffer sideBufferPool[AZA_MAX_SIDE_BUFFERS] = {{}};
thread_local size_t sideBufferCapacity[AZA_MAX_SIDE_BUFFERS] = {0};
thread_local size_t sideBuffersInUse = 0;

static azaBuffer azaPushSideBuffer(size_t frames, size_t channels, size_t samplerate) {
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
	buffer->channels = channels;
	buffer->samplerate = samplerate;
	if (*capacity < capacityNeeded) {
		azaBufferInit(buffer);
		*capacity = capacityNeeded;
	}
	sideBuffersInUse++;
	return *buffer;
}

static void azaPopSideBuffer() {
	assert(sideBuffersInUse > 0);
	sideBuffersInUse--;
}



static int azaCheckBuffer(azaBuffer buffer) {
	if (buffer.samples == NULL) {
		return AZA_ERROR_NULL_POINTER;
	}
	if (buffer.channels < 1) {
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
	data->samples = (float*)malloc(sizeof(float) * data->frames * data->channels);
	data->stride = data->channels;
	return AZA_SUCCESS;
}

int azaBufferDeinit(azaBuffer *data) {
	if (data->samples != NULL) {
		free(data->samples);
		return AZA_SUCCESS;
	}
	return AZA_ERROR_NULL_POINTER;
}



void azaBufferMix(azaBuffer dst, float volumeDst, azaBuffer src, float volumeSrc) {
	assert(dst.frames == src.frames);
	assert(dst.stride == src.stride);
	assert(dst.channels == src.channels);
	if AZA_UNLIKELY(volumeDst == 0.0f && volumeSrc == 0.0f) {
		for (size_t i = 0; i < dst.frames*dst.channels; i++) {
			dst.samples[i] = 0.0f;
		}
	} else if (volumeDst == 1.0f && volumeSrc == 1.0f) {
		for (size_t i = 0; i < dst.frames*dst.channels; i++) {
			dst.samples[i] = dst.samples[i] + src.samples[i];
		}
	} else if AZA_LIKELY(volumeDst == 1.0f) {
		for (size_t i = 0; i < dst.frames*dst.channels; i++) {
			dst.samples[i] = dst.samples[i] + src.samples[i] * volumeSrc;
		}
	} else if (volumeSrc == 1.0f) {
		for (size_t i = 0; i < dst.frames*dst.channels; i++) {
			dst.samples[i] = dst.samples[i] * volumeDst + src.samples[i];
		}
	} else {
		for (size_t i = 0; i < dst.frames*dst.channels; i++) {
			dst.samples[i] = dst.samples[i] * volumeDst + src.samples[i] * volumeSrc;
		}
	}
}

void azaBufferCopyChannel(azaBuffer dst, size_t channelDst, azaBuffer src, size_t channelSrc) {
	assert(dst.frames == src.frames);
	assert(channelDst < dst.channels);
	assert(channelSrc < src.channels);
	if (dst.stride == 1 && src.stride == 1) {
		memcpy(dst.samples, src.samples, sizeof(float) * dst.frames);
	} else if (dst.stride == 1) {
		for (size_t i = 0; i < dst.frames; i++) {
			dst.samples[i] = src.samples[i * src.stride + channelSrc];
		}
	} else if (src.stride == 1) {
		for (size_t i = 0; i < dst.frames; i++) {
			dst.samples[i * dst.stride + channelDst] = src.samples[i];
		}
	} else {
		for (size_t i = 0; i < dst.frames; i++) {
			dst.samples[i * dst.stride + channelDst] = src.samples[i * src.stride + channelSrc];
		}
	}
}



int azaDSP(azaBuffer buffer, azaDSPData *data) {
	switch (data->kind) {
		case AZA_DSP_RMS: return azaRms(buffer, (azaRmsData*)data);
		case AZA_DSP_FILTER: return azaFilter(buffer, (azaFilterData*)data);
		case AZA_DSP_LOOKAHEAD_LIMITER: return azaLookaheadLimiter(buffer, (azaLookaheadLimiterData*)data);
		case AZA_DSP_COMPRESSOR: return azaCompressor(buffer, (azaCompressorData*)data);
		case AZA_DSP_DELAY: return azaDelay(buffer, (azaDelayData*)data);
		case AZA_DSP_REVERB: return azaReverb(buffer, (azaReverbData*)data);
		case AZA_DSP_SAMPLER: return azaSampler(buffer, (azaSamplerData*)data);
		case AZA_DSP_GATE: return azaGate(buffer, (azaGateData*)data);
		default: return AZA_ERROR_INVALID_DSP_STRUCT;
	}
}



void azaRmsDataInit(azaRmsData *data) {
	data->header.kind = AZA_DSP_RMS;
	data->header.structSize = sizeof(*data);

	data->squared = 0.0f;
	for (int i = 0; i < AZAUDIO_RMS_SAMPLES; i++) {
		data->buffer[i] = 0.0f;
	}
	data->index = 0;
}

int azaRms(azaBuffer buffer, azaRmsData *data) {
	if (data == NULL) {
		return AZA_ERROR_NULL_POINTER;
	} else {
		int err = azaCheckBuffer(buffer);
		if (err) return err;
	}
	for (size_t c = 0; c < buffer.channels; c++) {
		azaRmsData *datum = &data[c];

		for (size_t i = 0; i < buffer.frames; i++) {
			size_t s = i * buffer.stride + c;
			datum->squared -= datum->buffer[datum->index];
			datum->buffer[datum->index] = buffer.samples[s] * buffer.samples[s];
			datum->squared += datum->buffer[datum->index];
			// Deal with potential rounding errors making sqrtf emit NaNs
			if (datum->squared < 0.0f) datum->squared = 0.0f;

			if (++datum->index >= AZAUDIO_RMS_SAMPLES)
				datum->index = 0;

			buffer.samples[s] = sqrtf(datum->squared/AZAUDIO_RMS_SAMPLES);
		}
	}
	if (data->header.pNext) {
		return azaDSP(buffer, data->header.pNext);
	}
	return AZA_SUCCESS;
}



static float azaCubicLimiterSample(float sample) {
	if (sample > 1.0f)
		sample = 1.0f;
	else if (sample < -1.0f)
		sample = -1.0f;
	else
		sample = sample;
	sample = 1.5 * sample - 0.5f * sample * sample * sample;
	return sample;
}

int azaCubicLimiter(azaBuffer buffer) {
	{
		int err = azaCheckBuffer(buffer);
		if (err) return err;
	}
	if (buffer.stride == buffer.channels) {
		for (size_t i = 0; i < buffer.frames*buffer.channels; i++) {
			buffer.samples[i] = azaCubicLimiterSample(buffer.samples[i]);
		}
	} else {
		for (size_t c = 0; c < buffer.channels; c++) {
			for (size_t i = 0; i < buffer.frames; i++) {
				size_t s = i * buffer.stride + c;
				buffer.samples[s] = azaCubicLimiterSample(buffer.samples[s]);
			}
		}
	}
	return AZA_SUCCESS;
}



void azaLookaheadLimiterDataInit(azaLookaheadLimiterData *data) {
	data->header.kind = AZA_DSP_LOOKAHEAD_LIMITER;
	data->header.structSize = sizeof(*data);

	memset(data->gainBuffer, 0, sizeof(float) * AZAUDIO_LOOKAHEAD_SAMPLES);
	memset(data->valBuffer, 0, sizeof(float) * AZAUDIO_LOOKAHEAD_SAMPLES);
	data->index = 0;
	data->sum = 0.0f;
}

int azaLookaheadLimiter(azaBuffer buffer, azaLookaheadLimiterData *data) {
	if (data == NULL) {
		return AZA_ERROR_NULL_POINTER;
	} else {
		int err = azaCheckBuffer(buffer);
		if (err) return err;
	}
	for (size_t c = 0; c < buffer.channels; c++) {
		azaLookaheadLimiterData *datum = &data[c];
		float amountOutput = aza_db_to_ampf(datum->gainOutput);

		for (size_t i = 0; i < buffer.frames; i++) {
			size_t s = i * buffer.stride + c;
			float peak = buffer.samples[s];
			float gain = datum->gainInput;
			if (peak < 0.0f)
				peak = -peak;
			peak = aza_amp_to_dbf(peak) + gain;
			if (peak < 0.0f)
				peak = 0.0f;
			datum->sum += peak - datum->gainBuffer[datum->index];
			float average = datum->sum / AZAUDIO_LOOKAHEAD_SAMPLES;
			if (average > peak) {
				datum->sum += average - peak;
				peak = average;
			}
			datum->gainBuffer[datum->index] = peak;

			datum->valBuffer[datum->index] = buffer.samples[s];

			datum->index = (datum->index+1)%AZAUDIO_LOOKAHEAD_SAMPLES;

			if (average > datum->gainBuffer[datum->index])
				gain -= average;
			else
				gain -= datum->gainBuffer[datum->index];
			float out = datum->valBuffer[datum->index] * aza_db_to_ampf(gain);
			if (out < -1.0f)
				out = -1.0f;
			else if (out > 1.0f)
				out = 1.0f;
			buffer.samples[s] = out * amountOutput;
		}
	}
	if (data->header.pNext) {
		return azaDSP(buffer, data->header.pNext);
	}
	return AZA_SUCCESS;
}



void azaFilterDataInit(azaFilterData *data) {
	data->header.kind = AZA_DSP_FILTER;
	data->header.structSize = sizeof(*data);

	data->outputs[0] = 0.0f;
	data->outputs[1] = 0.0f;
}

int azaFilter(azaBuffer buffer, azaFilterData *data) {
	if (data == NULL) {
		return AZA_ERROR_NULL_POINTER;
	} else {
		int err = azaCheckBuffer(buffer);
		if (err) return err;
	}
	for (size_t c = 0; c < buffer.channels; c++) {
		azaFilterData *datum = &data[c];
		switch (datum->kind) {
			case AZA_FILTER_HIGH_PASS: {
				float decay = clampf(expf(-AZA_TAU * (datum->frequency / (float)buffer.samplerate)), 0.0f, 1.0f);
				for (size_t i = 0; i < buffer.frames; i++) {
					size_t s = i * buffer.stride + c;
					datum->outputs[0] = buffer.samples[s] + decay * (datum->outputs[0] - buffer.samples[s]);
					buffer.samples[s] = buffer.samples[s] - datum->outputs[0];
				}
			} break;
			case AZA_FILTER_LOW_PASS: {
				float decay = clampf(expf(-AZA_TAU * (datum->frequency / (float)buffer.samplerate)), 0.0f, 1.0f);
				for (size_t i = 0; i < buffer.frames; i++) {
					size_t s = i * buffer.stride + c;
					datum->outputs[0] = buffer.samples[s] + decay * (datum->outputs[0] - buffer.samples[s]);
					buffer.samples[s] = datum->outputs[0];
				}
			} break;
			case AZA_FILTER_BAND_PASS: {
				float decayLow = clampf(expf(-AZA_TAU * (datum->frequency / (float)buffer.samplerate)), 0.0f, 1.0f);
				float decayHigh = clampf(expf(-AZA_TAU * (datum->frequency / (float)buffer.samplerate)), 0.0f, 1.0f);
				for (size_t i = 0; i < buffer.frames; i++) {
					size_t s = i * buffer.stride + c;
					datum->outputs[0] = buffer.samples[s] + decayLow * (datum->outputs[0] - buffer.samples[s]);
					datum->outputs[1] = datum->outputs[0] + decayHigh * (datum->outputs[1] - datum->outputs[0]);
					buffer.samples[s] = (datum->outputs[0] - datum->outputs[1]) * 2.0f;
				}
			} break;
		}
	}
	if (data->header.pNext) {
		return azaDSP(buffer, data->header.pNext);
	}
	return AZA_SUCCESS;
}



void azaCompressorDataInit(azaCompressorData *data) {
	data->header.kind = AZA_DSP_COMPRESSOR;
	data->header.structSize = sizeof(*data);

	azaRmsDataInit(&data->rmsData);
	data->attenuation = 0.0f;
	data->gain = 0.0f;
}

int azaCompressor(azaBuffer buffer, azaCompressorData *data) {
	if (data == NULL) {
		return AZA_ERROR_NULL_POINTER;
	} else {
		int err = azaCheckBuffer(buffer);
		if (err) return err;
	}
	azaBuffer sideBuffer = azaPushSideBuffer(buffer.frames, 1, buffer.samplerate);
	for (size_t c = 0; c < buffer.channels; c++) {
		azaCompressorData *datum = &data[c];
		float t = (float)buffer.samplerate / 1000.0f;
		float attackFactor = expf(-1.0f / (datum->attack * t));
		float decayFactor = expf(-1.0f / (datum->decay * t));
		float overgainFactor;
		if (datum->ratio > 1.0f) {
			overgainFactor = (1.0f - 1.0f / datum->ratio);
		} else if (datum->ratio < 0.0f) {
			overgainFactor = -datum->ratio;
		} else {
			overgainFactor = 0.0f;
		}

		for (size_t i = 0; i < buffer.frames; i++) {
			size_t s = i * buffer.stride + c;
			sideBuffer.samples[i] = buffer.samples[s];
		}
		azaRms(sideBuffer, &datum->rmsData);
		for (size_t i = 0; i < buffer.frames; i++) {
			size_t s = i * buffer.stride + c;
			
			float rms = aza_amp_to_dbf(sideBuffer.samples[i]);
			if (rms < -120.0f) rms = -120.0f;
			if (rms > datum->attenuation) {
				datum->attenuation = rms + attackFactor * (datum->attenuation - rms);
			} else {
				datum->attenuation = rms + decayFactor * (datum->attenuation - rms);
			}
			float gain;
			if (datum->attenuation > datum->threshold) {
				gain = overgainFactor * (datum->threshold - datum->attenuation);
			} else {
				gain = 0.0f;
			}
			datum->gain = gain;
			buffer.samples[s] = buffer.samples[s] * aza_db_to_ampf(gain);
		}
	}
	azaPopSideBuffer();
	if (data->header.pNext) {
		return azaDSP(buffer, data->header.pNext);
	}
	return AZA_SUCCESS;
}



static void azaDelayDataHandleBufferResizes(azaDelayData *data, size_t delaySamples) {
	if (data->delaySamples >= delaySamples) {
		if (data->index > delaySamples) {
			data->index = 0;
		}
		data->delaySamples = delaySamples;
		return;
	} else if (data->capacity >= delaySamples) {
		data->delaySamples = delaySamples;
		return;
	}
	// Have to realloc buffer
	size_t newCapacity = aza_grow(data->capacity, delaySamples, 1024);
	float *newBuffer = malloc(sizeof(float) * newCapacity);
	if (data->buffer) {
		memcpy(newBuffer, data->buffer, sizeof(float) * data->delaySamples);
		free(data->buffer);
	}
	data->buffer = newBuffer;
	for (size_t i = data->delaySamples; i < delaySamples; i++) {
		data->buffer[i] = 0.0f;
	}
	data->delaySamples = delaySamples;
}

void azaDelayDataInit(azaDelayData *data) {
	data->header.kind = AZA_DSP_DELAY;
	data->header.structSize = sizeof(*data);

	data->buffer = NULL;
	data->capacity = 0;
	data->delaySamples = 0;
	data->index = 0;
	azaDelayDataHandleBufferResizes(data, aza_ms_to_samples(data->delay, 48000));
}

void azaDelayDataDeinit(azaDelayData *data) {
	free(data->buffer);
}

int azaDelay(azaBuffer buffer, azaDelayData *data) {
	if (data == NULL) {
		return AZA_ERROR_NULL_POINTER;
	} else {
		int err = azaCheckBuffer(buffer);
		if (err) return err;
	}
	azaBuffer sideBuffer = azaPushSideBuffer(buffer.frames, 1, buffer.samplerate);
	for (size_t c = 0; c < buffer.channels; c++) {
		azaDelayData *datum = &data[c];
		size_t delaySamples = aza_ms_to_samples(datum->delay, buffer.samplerate);
		azaDelayDataHandleBufferResizes(datum, delaySamples);
		float amount = aza_db_to_ampf(datum->gain);
		float amountDry = aza_db_to_ampf(datum->gainDry);
		size_t index = datum->index;
		for (size_t i = 0; i < buffer.frames; i++) {
			size_t s = i * buffer.stride + c;
			sideBuffer.samples[i] = buffer.samples[s] + datum->buffer[index] * datum->feedback;
			index = (index+1) % delaySamples;
		}
		if (datum->wetEffects) {
			int err = azaDSP(sideBuffer, datum->wetEffects);
			if (err) return err;
		}
		index = datum->index;
		for (size_t i = 0; i < buffer.frames; i++) {
			size_t s = i * buffer.stride + c;
			datum->buffer[index] = sideBuffer.samples[i];
			index = (index+1) % delaySamples;
			buffer.samples[s] = datum->buffer[index] * amount + buffer.samples[s] * amountDry;
		}
		datum->index = index;
	}
	azaPopSideBuffer();
	if (data->header.pNext) {
		return azaDSP(buffer, data->header.pNext);
	}
	return AZA_SUCCESS;
}



void azaReverbDataInit(azaReverbData *data) {
	data->header.kind = AZA_DSP_REVERB;
	data->header.structSize = sizeof(*data);

	float delays[AZAUDIO_REVERB_DELAY_COUNT] = {
		AZA_SAMPLES_TO_MS(1557, 48000),
		AZA_SAMPLES_TO_MS(1617, 48000),
		AZA_SAMPLES_TO_MS(1491, 48000),
		AZA_SAMPLES_TO_MS(1422, 48000),
		AZA_SAMPLES_TO_MS(1277, 48000),
		AZA_SAMPLES_TO_MS(1356, 48000),
		AZA_SAMPLES_TO_MS(1188, 48000),
		AZA_SAMPLES_TO_MS(1116, 48000),
		AZA_SAMPLES_TO_MS(2111, 48000),
		AZA_SAMPLES_TO_MS(2133, 48000),
		AZA_SAMPLES_TO_MS( 673, 48000),
		AZA_SAMPLES_TO_MS( 556, 48000),
		AZA_SAMPLES_TO_MS( 441, 48000),
		AZA_SAMPLES_TO_MS( 341, 48000),
		AZA_SAMPLES_TO_MS( 713, 48000),
	};
	for (int i = 0; i < AZAUDIO_REVERB_DELAY_COUNT; i++) {
		data->delayDatas[i].delay = delays[i] + data->delay;
		data->delayDatas[i].gain = 0.0f;
		data->delayDatas[i].gainDry = 0.0f;
		azaDelayDataInit(&data->delayDatas[i]);
		data->filterDatas[i].kind = AZA_FILTER_LOW_PASS;
		azaFilterDataInit(&data->filterDatas[i]);
	}
}

void azaReverbDataDeinit(azaReverbData *data) {
	for (int i = 0; i < AZAUDIO_REVERB_DELAY_COUNT; i++) {
		azaDelayDataDeinit(&data->delayDatas[i]);
	}
}

int azaReverb(azaBuffer buffer, azaReverbData *data) {
	if (data == NULL) {
		return AZA_ERROR_NULL_POINTER;
	} else {
		int err = azaCheckBuffer(buffer);
		if (err) return err;
	}
	azaBuffer sideBufferCombined = azaPushSideBuffer(buffer.frames, 1, buffer.samplerate);
	azaBuffer sideBufferEarly = azaPushSideBuffer(buffer.frames, 1, buffer.samplerate);
	azaBuffer sideBufferDiffuse = azaPushSideBuffer(buffer.frames, 1, buffer.samplerate);
	for (size_t c = 0; c < buffer.channels; c++) {
		azaReverbData *datum = &data[c];
		float feedback = 0.985f - (0.2f / datum->roomsize);
		float color = datum->color * 4000.0f;
		float amount = aza_db_to_ampf(datum->gain);
		float amountDry = aza_db_to_ampf(datum->gainDry);

		memset(sideBufferCombined.samples, 0, sizeof(float) * buffer.frames);
		for (int tap = 0; tap < AZAUDIO_REVERB_DELAY_COUNT*2/3; tap++) {
			datum->delayDatas[tap].feedback = feedback;
			datum->filterDatas[tap].frequency = color;
			azaBufferCopyChannel(sideBufferEarly, 0, buffer, c);
			azaFilter(sideBufferEarly, &datum->filterDatas[tap]);
			azaDelay(sideBufferEarly, &datum->delayDatas[tap]);
			azaBufferMix(sideBufferCombined, 1.0f, sideBufferEarly, 1.0f / (float)AZAUDIO_REVERB_DELAY_COUNT);
		}
		for (int tap = AZAUDIO_REVERB_DELAY_COUNT*2/3; tap < AZAUDIO_REVERB_DELAY_COUNT; tap++) {
			datum->delayDatas[tap].feedback = (float)(tap+8) / (AZAUDIO_REVERB_DELAY_COUNT + 8.0f);
			datum->filterDatas[tap].frequency = color*4.0f;
			azaBufferCopyChannel(sideBufferDiffuse, 0, sideBufferCombined, 0);
			azaFilter(sideBufferDiffuse, &datum->filterDatas[tap]);
			azaDelay(sideBufferDiffuse, &datum->delayDatas[tap]);
			azaBufferMix(sideBufferCombined, 1.0f, sideBufferDiffuse, 1.0f / (float)AZAUDIO_REVERB_DELAY_COUNT);
		}
		for (size_t i = 0; i < buffer.frames; i++) {
			size_t s = i * buffer.stride + c;
			buffer.samples[s] = sideBufferCombined.samples[i] * amount + buffer.samples[s] * amountDry;
		}
	}
	azaPopSideBuffer();
	azaPopSideBuffer();
	azaPopSideBuffer();
	if (data->header.pNext) {
		return azaDSP(buffer, data->header.pNext);
	}
	return AZA_SUCCESS;
}



int azaSamplerDataInit(azaSamplerData *data) {
	data->header.kind = AZA_DSP_SAMPLER;
	data->header.structSize = sizeof(*data);

	if (data->buffer == NULL) {
		AZA_PRINT_ERR("azaSamplerDataInit error: Sampler initialized without a buffer!");
		return AZA_ERROR_NULL_POINTER;
	}
	data->frame = 0;
	data->s = data->speed;
	// Starting at zero ensures click-free playback no matter what
	data->g = 0.0f;
	return AZA_SUCCESS;
}

int azaSampler(azaBuffer buffer, azaSamplerData *data) {
	if (data == NULL) {
		return AZA_ERROR_NULL_POINTER;
	} else {
		int err = azaCheckBuffer(buffer);
		if (err) return err;
	}
	float transition = expf(-1.0f / (AZAUDIO_SAMPLER_TRANSITION_FRAMES));
	for (size_t c = 0; c < buffer.channels; c++) {
		azaSamplerData *datum = &data[c];
		float samplerateFactor = (float)buffer.samplerate / (float)datum->buffer->samplerate;

		for (size_t i = 0; i < buffer.frames; i++) {
			size_t s = i * buffer.stride + c;

			datum->s = datum->speed + transition * (datum->s - datum->speed);
			datum->g = datum->gain + transition * (datum->g - datum->gain);

			// Adjust for different samplerates
			float speed = datum->s * samplerateFactor;
			float volume = aza_db_to_ampf(datum->g);

			float sample = 0.0f;

			/* Lanczos
			int t = (int)datum->frame + (int)datum->s;
			for (int i = (int)datum->frame-2; i <= t+2; i++) {
				float x = datum->frame - (float)(i);
				sample += datum->buffer->samples[i % datum->buffer->frames] * sinc(x) * sinc(x/3);
			}
			*/

			float frameFraction = datum->frame - (float)((int)datum->frame);
			if (speed <= 1.0f) {
				// Cubic
				float abcd[4];
				int ii = (int)datum->frame + datum->buffer->frames - 2;
				for (int i = 0; i < 4; i++) {
					abcd[i] = datum->buffer->samples[ii++ % datum->buffer->frames];
				}
				sample = cubic(abcd[0], abcd[1], abcd[2], abcd[3], frameFraction);
			} else {
				// Oversampling
				float total = 0.0f;
				total += datum->buffer->samples[(int)datum->frame % datum->buffer->frames] * (1.0f - frameFraction);
				for (int i = 1; i < (int)datum->speed; i++) {
					total += datum->buffer->samples[((int)datum->frame + i) % datum->buffer->frames];
				}
				total += datum->buffer->samples[((int)datum->frame + (int)datum->speed) % datum->buffer->frames] * frameFraction;
				sample = total / (float)((int)datum->speed);
			}

			/* Linear
			int t = (int)datum->frame + (int)datum->s;
			for (int i = (int)datum->frame; i <= t+1; i++) {
				float x = datum->frame - (float)(i);
				sample += datum->buffer->samples[i % datum->buffer->frames] * linc(x);
			}
			*/

			buffer.samples[s] = sample * volume;
			datum->frame = datum->frame + datum->s;
			if ((int)datum->frame > datum->buffer->frames) {
				datum->frame -= (float)datum->buffer->frames;
			}
		}
	}
	if (data->header.pNext) {
		return azaDSP(buffer, data->header.pNext);
	}
	return AZA_SUCCESS;
}



void azaGateDataInit(azaGateData *data) {
	data->header.kind = AZA_DSP_GATE;
	data->header.structSize = sizeof(*data);

	azaRmsDataInit(&data->rms);
	data->attenuation = 0.0f;
	data->gain = 0.0f;
}

int azaGate(azaBuffer buffer, azaGateData *data) {
	azaBuffer sideBuffer = azaPushSideBuffer(buffer.frames, 1, buffer.samplerate);
	for (size_t c = 0; c < buffer.channels; c++) {
		azaGateData *datum = &data[c];
		float t = (float)buffer.samplerate / 1000.0f;
		float attackFactor = expf(-1.0f / (datum->attack * t));
		float decayFactor = expf(-1.0f / (datum->decay * t));

		azaBufferCopyChannel(sideBuffer, 0, buffer, c);
		
		if (datum->activationEffects) {
			int err = azaDSP(sideBuffer, datum->activationEffects);
			if (err) return err;
		}
		
#if 0
		azaBufferCopyChannel(buffer, c, sideBuffer, 0);
		if (c == 0) {
			azaRms(sideBuffer, &datum->rms);
			AZA_PRINT_INFO("rms: %fdB\n", aza_amp_to_dbf(sideBuffer.samples[sideBuffer.frames-1]));
		}
#else
		azaRms(sideBuffer, &datum->rms);
		for (size_t i = 0; i < buffer.frames; i++) {
			size_t s = i * buffer.stride + c;

			float rms = aza_amp_to_dbf(sideBuffer.samples[i]);
			if (rms < -120.0f) rms = -120.0f;
			if (rms > datum->threshold) {
				datum->attenuation = rms + attackFactor * (datum->attenuation - rms);
			} else {
				datum->attenuation = rms + decayFactor * (datum->attenuation - rms);
			}
			float gain;
			if (datum->attenuation > datum->threshold) {
				gain = 0.0f;
			} else {
				gain = -10.0f * (datum->threshold - datum->attenuation);
			}
			datum->gain = gain;
			buffer.samples[s] = buffer.samples[s] * aza_db_to_ampf(gain);
		}
#endif
	}
	azaPopSideBuffer();
	if (data->header.pNext) {
		return azaDSP(buffer, data->header.pNext);
	}
	return AZA_SUCCESS;
}

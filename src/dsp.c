/*
	File: dsp.c
	Author: Philip Haynes
*/

#include "audio.h"

#include "helpers.h"

#include <stdlib.h>

extern int azaError;

int azaBufferInit(azaBuffer *data) {
	if (data->frames < 1) {
		azaError = AZA_ERROR_INVALID_FRAME_COUNT;
		data->samples = NULL;
		return azaError;
	}
	data->samples = (float*)malloc(sizeof(float) * data->frames);
	azaError = AZA_SUCCESS;
	return azaError;
}

int azaBufferClean(azaBuffer *data) {
	if (data->samples != NULL) {
		free(data->samples);
		azaError = AZA_SUCCESS;
		return azaError;
	}
	azaError = AZA_ERROR_NULL_POINTER;
	return azaError;
}

void azaRmsDataInit(azaRmsData *data) {
	data->squared = 0.0f;
	for (int i = 0; i < AZAUDIO_RMS_SAMPLES; i++) {
		data->buffer[i] = 0.0f;
	}
	data->index = 0;
}

void azaLookaheadLimiterDataInit(azaLookaheadLimiterData *data) {
	for (int i = 0; i < AZAUDIO_LOOKAHEAD_SAMPLES; i++) {
		data->gainBuffer[i] = 0.0f;
		data->valBuffer[i] = 0.0f;
	}
	data->index = 0;
	data->sum = 0.0f;
}

void azaLowPassDataInit(azaLowPassData *data) {
	data->output = 0.0f;
}

void azaHighPassDataInit(azaHighPassData *data) {
	data->output = 0.0f;
}

void azaCompressorDataInit(azaCompressorData *data) {
	azaRmsDataInit(&data->rms);
	data->attenuation = 0.0f;
}

void azaDelayDataInit(azaDelayData *data) {
	data->buffer = (float*)malloc(sizeof(float) * data->samples);
	for (int i = 0; i < data->samples; i++) {
		data->buffer[i] = 0.0f;
	}
	data->index = 0;
}

void azaDelayDataClean(azaDelayData *data) {
	free(data->buffer);
}

void azaReverbDataInit(azaReverbData *data) {
	int samples[AZAUDIO_REVERB_DELAY_COUNT] = {1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116, 2111, 2133, 673, 556, 441, 341, 713};
	for (int i = 0; i < AZAUDIO_REVERB_DELAY_COUNT; i++) {
		data->delay[i].samples = samples[i] + data->samplesOffset;
		azaDelayDataInit(&data->delay[i]);
		azaLowPassDataInit(&data->lowPass[i]);
	}
}

int azaSamplerDataInit(azaSamplerData *data) {
	if (data->buffer == NULL) {
		azaError = AZA_ERROR_NULL_POINTER;
		azaPrint("Sampler initialized without a buffer!");
		return azaError;
	}
	data->frame = 0;
	data->s = 0.0f; // Starting at zero ensures click-free playback no matter what
	data->g = 0.0f;
	azaError = AZA_SUCCESS;
	return azaError;
}

void azaReverbDataClean(azaReverbData *data) {
	for (int i = 0; i < AZAUDIO_REVERB_DELAY_COUNT; i++) {
		azaDelayDataClean(&data->delay[i]);
	}
}

int azaCubicLimiter(const float *input, float *output, int frames, int channels) {
	if (input == NULL || output == NULL) {
		azaError = AZA_ERROR_NULL_POINTER;
		return azaError;
	}
	if (channels < 1) {
		azaError = AZA_ERROR_INVALID_CHANNEL_COUNT;
		return azaError;
	}
	if (frames < 1) {
		azaError = AZA_ERROR_INVALID_FRAME_COUNT;
		return azaError;
	}
	for (int i = 0; i < frames*channels; i++) {
		if (input[i] > 1.0f)
			output[i] = 1.0f;
		else if (input[i] < -1.0f)
			output[i] = -1.0f;
		else
			output[i] = input[i];
		output[i] = 1.5 * output[i] - 0.5f * output[i] * output[i] * output[i];
	}
	azaError = AZA_SUCCESS;
	return azaError;
}

int azaRms(const float *input, float *output, azaRmsData *data, int frames, int channels) {
	if (input == NULL || output == NULL || data == NULL) {
		azaError = AZA_ERROR_NULL_POINTER;
		return azaError;
	}
	if (channels < 1) {
		azaError = AZA_ERROR_INVALID_CHANNEL_COUNT;
		return azaError;
	}
	if (frames < 1) {
		azaError = AZA_ERROR_INVALID_FRAME_COUNT;
		return azaError;
	}
	for (int i = 0; i < frames*channels; i++) {
		azaRmsData *datum = &data[i % channels];

		datum->squared -= datum->buffer[datum->index];
		datum->buffer[datum->index] = input[i] * input[i];
		datum->squared += datum->buffer[datum->index++];
		if (datum->index >= AZAUDIO_RMS_SAMPLES)
			datum->index = 0;

		output[i] = sqrtf(datum->squared/AZAUDIO_RMS_SAMPLES);
	}
	azaError = AZA_SUCCESS;
	return azaError;
}

int azaLookaheadLimiter(const float *input, float *output, azaLookaheadLimiterData *data, int frames, int channels) {
	if (input == NULL || output == NULL || data == NULL) {
		azaError = AZA_ERROR_NULL_POINTER;
		return azaError;
	}
	if (channels < 1) {
		azaError = AZA_ERROR_INVALID_CHANNEL_COUNT;
		return azaError;
	}
	if (frames < 1) {
		azaError = AZA_ERROR_INVALID_FRAME_COUNT;
		return azaError;
	}
	for (int i = 0; i < frames*channels; i++) {
		azaLookaheadLimiterData *datum = &data[i % channels];

		float peak = input[i];
		float gain = datum->gain;
		if (peak < 0.0f)
			peak = -peak;
		peak = log10f(peak)*20.0f + gain;
		if (peak < 0.0f)
			peak = 0.0f;
		datum->sum += peak - datum->gainBuffer[datum->index];
		float average = datum->sum / AZAUDIO_LOOKAHEAD_SAMPLES;
		if (average > peak) {
			datum->sum += average - peak;
			peak = average;
		}
		datum->gainBuffer[datum->index] = peak;

		datum->valBuffer[datum->index] = input[i];

		datum->index = (datum->index+1)%AZAUDIO_LOOKAHEAD_SAMPLES;

		if (average > datum->gainBuffer[datum->index])
			gain -= average;
		else
			gain -= datum->gainBuffer[datum->index];
		float out = datum->valBuffer[datum->index] * powf(10.0f,gain/20.0f);
		if (out < -1.0f)
			out = -1.0f;
		else if (out > 1.0f)
			out = 1.0f;
		output[i] = out;
	}
	azaError = AZA_SUCCESS;
	return azaError;
}

int azaLowPass(const float *input, float *output, azaLowPassData *data, int frames, int channels) {
	if (input == NULL || output == NULL || data == NULL) {
		azaError = AZA_ERROR_NULL_POINTER;
		return azaError;
	}
	if (channels < 1) {
		azaError = AZA_ERROR_INVALID_CHANNEL_COUNT;
		return azaError;
	}
	if (frames < 1) {
		azaError = AZA_ERROR_INVALID_FRAME_COUNT;
		return azaError;
	}

	for (int i = 0; i < frames*channels; i++) {
		azaLowPassData *datum = &data[i % channels];

		float amount = expf(-1.0f * (datum->frequency / datum->samplerate));
		if (amount > 1.0f) amount = 1.0f;
		datum->output = input[i] + amount * (datum->output - input[i]);
		output[i] = datum->output;
	}
	azaError = AZA_SUCCESS;
	return azaError;
}

int azaHighPass(const float *input, float *output, azaHighPassData *data, int frames, int channels) {
	if (input == NULL || output == NULL || data == NULL) {
		azaError = AZA_ERROR_NULL_POINTER;
		return azaError;
	}
	if (channels < 1) {
		azaError = AZA_ERROR_INVALID_CHANNEL_COUNT;
		return azaError;
	}
	if (frames < 1) {
		azaError = AZA_ERROR_INVALID_FRAME_COUNT;
		return azaError;
	}

	for (int i = 0; i < frames*channels; i++) {
		azaHighPassData *datum = &data[i % channels];

		float amount = expf(-8.0f * (datum->frequency / datum->samplerate));
		datum->output = input[i] + amount * (datum->output - input[i]);
		output[i] = input[i] - datum->output;
	}
	azaError = AZA_SUCCESS;
	return azaError;
}

int azaCompressor(const float *input, float *output, azaCompressorData *data, int frames, int channels) {
	if (input == NULL || output == NULL || data == NULL) {
		azaError = AZA_ERROR_NULL_POINTER;
		return azaError;
	}
	if (channels < 1) {
		azaError = AZA_ERROR_INVALID_CHANNEL_COUNT;
		return azaError;
	}
	if (frames < 1) {
		azaError = AZA_ERROR_INVALID_FRAME_COUNT;
		return azaError;
	}

	for (int i = 0; i < frames*channels; i++) {
		azaCompressorData *datum = &data[i % channels];

		float rms;
		azaRms(&input[i], &rms, &datum->rms, 1, 1);
		rms = log10f(rms)*20.0f;
		float t = datum->samplerate / 1000.0f; // millisecond units
		float mult;
		if (datum->ratio > 0.0f) {
			mult = (1.0f - 1.0f / datum->ratio);
		} else {
			mult = -datum->ratio;
		}
		if (rms > datum->attenuation) {
			datum->attenuation = rms + expf(-1.0f / (datum->attack * t)) * (datum->attenuation - rms);
		} else {
			datum->attenuation = rms + expf(-1.0f / (datum->decay * t)) * (datum->attenuation - rms);
		}
		float gain;
		if (datum->attenuation > datum->threshold) {
			gain = mult * (datum->threshold - datum->attenuation);
		} else {
			gain = 0.0f;
		}
		datum->gain = gain;
		output[i] = input[i] * powf(10.0f,gain/20.0f);
	}
	azaError = AZA_SUCCESS;
	return azaError;
}

int azaDelay(const float *input, float *output, azaDelayData *data, int frames, int channels) {
	if (input == NULL || output == NULL || data == NULL) {
		azaError = AZA_ERROR_NULL_POINTER;
		return azaError;
	}
	if (channels < 1) {
		azaError = AZA_ERROR_INVALID_CHANNEL_COUNT;
		return azaError;
	}
	if (frames < 1) {
		azaError = AZA_ERROR_INVALID_FRAME_COUNT;
		return azaError;
	}

	for (int i = 0; i < frames*channels; i++) {
		azaDelayData *datum = &data[i % channels];

		datum->buffer[datum->index] = input[i] + datum->buffer[datum->index] * datum->feedback;
		datum->index++;
		if (datum->index >= datum->samples) {
			datum->index = 0;
		}
		output[i] = datum->buffer[datum->index] * datum->amount + input[i];
	}
	azaError = AZA_SUCCESS;
	return azaError;
}

int azaReverb(const float *input, float *output, azaReverbData *data, int frames, int channels) {
	if (input == NULL || output == NULL || data == NULL) {
		azaError = AZA_ERROR_NULL_POINTER;
		return azaError;
	}
	if (channels < 1) {
		azaError = AZA_ERROR_INVALID_CHANNEL_COUNT;
		return azaError;
	}
	if (frames < 1) {
		azaError = AZA_ERROR_INVALID_FRAME_COUNT;
		return azaError;
	}

	for (int i = 0; i < frames*channels; i++) {
		azaReverbData *datum = &data[i % channels];

		float out = input[i];
		float feedback = 0.98f - (0.2f / datum->roomsize);
		float color = datum->color * 4000.0f;
		for (int ii = 0; ii < AZAUDIO_REVERB_DELAY_COUNT*2/3; ii++) {
			datum->delay[ii].feedback = feedback;
			datum->delay[ii].amount = 1.0f;
			datum->lowPass[ii].samplerate = (float)AZA_SAMPLERATE;
			datum->lowPass[ii].frequency = color;
			float early = input[i];
			azaLowPass(&early, &early, &datum->lowPass[ii], 1, 1);
			azaDelay(&early, &early, &datum->delay[ii], 1, 1);
			out += (early - input[i]) / (float)AZAUDIO_REVERB_DELAY_COUNT;
		}
		for (int ii = AZAUDIO_REVERB_DELAY_COUNT*2/3; ii < AZAUDIO_REVERB_DELAY_COUNT; ii++) {
			datum->delay[ii].feedback = (float)(ii+8) / (AZAUDIO_REVERB_DELAY_COUNT + 8.0f);
			datum->delay[ii].amount = 1.0f;
			datum->lowPass[ii].samplerate = (float)AZA_SAMPLERATE;
			datum->lowPass[ii].frequency = color*4.0f;
			float diffuse = out;
			azaLowPass(&diffuse, &diffuse, &datum->lowPass[ii], 1, 1);
			azaDelay(&diffuse, &diffuse, &datum->delay[ii], 1, 1);
			out += (diffuse - out) / (float)AZAUDIO_REVERB_DELAY_COUNT;
		}
		out *= datum->amount;
		output[i] = out + input[i];
	}
	azaError = AZA_SUCCESS;
	return azaError;
}

int azaSampler(const float *input, float *output, azaSamplerData *data, int frames, int channels) {
	if (output == NULL || data == NULL) {
		azaError = AZA_ERROR_NULL_POINTER;
		return azaError;
	}
	if (channels < 1) {
		azaError = AZA_ERROR_INVALID_CHANNEL_COUNT;
		return azaError;
	}
	if (frames < 1) {
		azaError = AZA_ERROR_INVALID_FRAME_COUNT;
		return azaError;
	}

	for (int i = 0; i < frames*channels; i++) {
		azaSamplerData *datum = &data[i % channels];

		datum->s = datum->speed + expf(-1.0f / (AZAUDIO_SAMPLER_TRANSITION_FRAMES)) * (datum->s - datum->speed);
		datum->g = datum->gain + expf(-1.0f / (AZAUDIO_SAMPLER_TRANSITION_FRAMES)) * (datum->g - datum->gain);


		float sample = 0.0f;

		/* Lanczos
		int t = (int)datum->frame + (int)datum->s;
		for (int i = (int)datum->frame-2; i <= t+2; i++) {
			float x = datum->frame - (float)(i);
			sample += datum->buffer->samples[i % datum->buffer->frames] * sinc(x) * sinc(x/3);
		}
		*/

		if (datum->speed <= 1.0f) {
			///* Cubic
			float abcd[4];
			int ii = (int)datum->frame-2;
			for (int i = 0; i < 4; i++) {
				abcd[i] = datum->buffer->samples[ii++ % datum->buffer->frames];
			}
			sample = cubic(abcd[0], abcd[1], abcd[2], abcd[3], datum->frame - (float)((int)datum->frame));
		} else {
			// Oversampling
			float bias = datum->frame - (float)((int)datum->frame);
			float total = 0.0f;
			total += datum->buffer->samples[(int)datum->frame % datum->buffer->frames] * (1.0f - bias);
			for (int i = 1; i < (int)datum->speed; i++) {
				total += datum->buffer->samples[((int)datum->frame + i) % datum->buffer->frames];
			}
			total += datum->buffer->samples[((int)datum->frame + (int)datum->speed) % datum->buffer->frames] * bias;
			sample = total / (float)((int)datum->speed);
		}
		//*/

		/* Linear
		int t = (int)datum->frame + (int)datum->s;
		for (int i = (int)datum->frame; i <= t+1; i++) {
			float x = datum->frame - (float)(i);
			sample += datum->buffer->samples[i % datum->buffer->frames] * linc(x);
		}
		*/

		output[i] = sample * datum->g;
		datum->frame = datum->frame + datum->s;
		if ((int)datum->frame > datum->buffer->frames) {
			datum->frame -= (float)datum->buffer->frames;
		}
	}
	azaError = AZA_SUCCESS;
	return azaError;
}
/*
	File: main.c
	Author: singularity
	Program for testing lookahead limiting.
*/

#include <stdlib.h>
#include <stdio.h>

#include "AzAudio/AzAudio.h"
#include "AzAudio/dsp.h"
#include "AzAudio/error.h"
#include "AzAudio/helpers.h"
#include "AzAudio/math.h"

azaLookaheadLimiter *limiter = NULL;
float angle = 0.0f;
float time = 0.0f;

int mixCallbackOutput(void *userdata, azaBuffer buffer) {
	float frameDelta = 1.0f / (float)buffer.samplerate;
	float timeDelta = (float)buffer.frames * frameDelta;
	int32_t clickFrame = (int32_t)((0.5f - time) * (float)buffer.samplerate);
	time += timeDelta;
	if (time > 1.0f) time -= 1.0f;
	float sineAmp = aza_db_to_ampf(-10.0f);
	for (int32_t i = 0; i < (int32_t)buffer.frames; i++) {
		float sample = sinf(angle) * sineAmp;
		// 2khz
		angle += frameDelta * 2000.0f * AZA_TAU;
		if (angle > AZA_TAU) angle -= AZA_TAU;
		if (i == clickFrame || i + 16 == clickFrame) sample = 1.0f;
		for (uint8_t c = 0; c < buffer.channelLayout.count; c++) {
			buffer.samples[i * buffer.stride + c] = sample;
		}
	}
	int err;
	if ((err = azaLookaheadLimiterProcess(limiter, buffer))) {
		char buffer[64];
		AZA_LOG_ERR("azaLookaheadLimiterProcess returned %s\n", azaErrorString(err, buffer, sizeof(buffer)));
		goto done;
	}
done:
	return err;
}

int main(int argumentCount, char** argumentValues) {
	int err = azaInit();
	if (err) {
		fprintf(stderr, "Failed to azaInit!\n");
		return 1;
	}

	azaStream streamOutput = {0};
	streamOutput.mixCallback = mixCallbackOutput;
	if ((err = azaStreamInitDefault(&streamOutput, AZA_OUTPUT, false)) != AZA_SUCCESS) {
		char buffer[64];
		fprintf(stderr, "Failed to init output stream! (%s)\n", azaErrorString(err, buffer, sizeof(buffer)));
		return 1;
	}
	uint8_t outputChannelCount = azaStreamGetChannelLayout(&streamOutput).count;

	limiter = azaMakeLookaheadLimiter((azaLookaheadLimiterConfig) {
		.gainInput  =  10.0f,
		.gainOutput = -10.0f,
	}, outputChannelCount);

	azaStreamSetActive(&streamOutput, 1);
	printf("Press ENTER to stop\n");
	getc(stdin);
	azaStreamDeinit(&streamOutput);

	azaFreeLookaheadLimiter(limiter);

	azaDeinit();
	return 0;
}

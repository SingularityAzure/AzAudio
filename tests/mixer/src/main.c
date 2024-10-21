/*
	File: main.c
	Author: singularity
	Program for testing sound spatialization.
*/

#include <stdlib.h>
#include <stdio.h>

#include "AzAudio/AzAudio.h"
#include "AzAudio/mixer.h"
#include "AzAudio/error.h"
#include "AzAudio/math.h"

#include <stb_vorbis.c>

// Master

azaMixer mixer;
azaLookaheadLimiter *limiter = NULL;

// Track 0

azaFilter *filter = NULL;
azaDSPUser dspSynth;
float gen[8] = {0.0f};
float freqs[8] = {
	// 25.0f,
	// 50.0f,
	// 75.0f,
	100.0f,
	125.0f,
	150.0f,
	175.0f,
	200.0f,
	300.0f,
	400.0f,
	500.0f,
	// 600.0f,
	// 700.0f,
	// 800.0f,
	// 900.0f,
};
float gains[8] = {
	1.0f,
	0.5f,
	0.25f,
	0.125f,
	0.0625f,
	0.03125f,
	0.015625f,
	0.0078125f,
};
float lfo = 0.0f;
int synthProcess(void *userdata, azaBuffer buffer) {
	float timestep = 1.0f / (float)buffer.samplerate;
	for (uint32_t i = 0; i < buffer.frames; i++) {
		float sample = 0.0f;
		for (uint32_t o = 0; o < sizeof(gen) / sizeof(float); o++) {
			sample += azaOscTriangle(gen[o]) * gains[o];
			gen[o] = azaWrap01f(gen[o] + timestep * freqs[o]);
		}
		sample *= (1.0f + azaOscSine(lfo)) * 0.5f;
		lfo = azaWrap01f(lfo + timestep);
		for (uint8_t c = 0; c < buffer.channelLayout.count; c++) {
			buffer.samples[i * buffer.stride + c] = sample;
		}
	}
	return AZA_SUCCESS;
}

// Track 1

azaBuffer bufferCat = {0};
azaSampler *samplerCat = NULL;
azaSpatialize **spatializeCat = NULL;
azaDSPUser dspCat;

#define PRINT_OBJECT_INFO 0

typedef struct Object {
	azaVec3 posPrev;
	azaVec3 pos;
	azaVec3 vel;
	azaVec3 target;
#if PRINT_OBJECT_INFO
	float timer;
#endif
} Object;

Object *objects;

int loadSoundFileIntoBuffer(azaBuffer *buffer, const char *filename) {
	int err;
	stb_vorbis *vorbis = stb_vorbis_open_filename(filename, &err, NULL);
	if (!vorbis) {
		fprintf(stderr, "Failed to load sound \"%s\": (%d)\n", filename, err);
		return 1;
	}
	buffer->frames = stb_vorbis_stream_length_in_samples(vorbis);
	stb_vorbis_info info = stb_vorbis_get_info(vorbis);
	printf("Sound \"%s\" has %u channels and a samplerate of %u\n", filename, info.channels, info.sample_rate);
	buffer->channelLayout.count = info.channels;
	buffer->samplerate = info.sample_rate;
	azaBufferInit(buffer, buffer->frames, buffer->channelLayout);
	stb_vorbis_get_samples_float_interleaved(vorbis, buffer->channelLayout.count, buffer->samples, buffer->frames * buffer->channelLayout.count);
	stb_vorbis_close(vorbis);
	return 0;
}

float randomf(float min, float max) {
	float val = (float)((uint32_t)rand());
	val /= (float)RAND_MAX;
	val = val * (max - min) + min;
	return val;
}

void updateObjects(uint32_t count, float timeDelta) {
	if (count == 0) return;
	float angleSize = AZA_TAU / (float)count;
	for (uint32_t i = 0; i < count; i++) {
		Object *object = &objects[i];
#if PRINT_OBJECT_INFO
		if (object->timer <= 0.0f) {
			printf("target = { %f, %f, %f }\n", object->target.x, object->target.y, object->target.z);
			printf("pos = { %f, %f, %f }\n", object->pos.x, object->pos.y, object->pos.z);
			printf("vel = { %f, %f, %f }\n", object->vel.x, object->vel.y, object->vel.z);
			object->timer += 0.5f;
		}
		object->timer -= timeDelta;
#endif
		if (azaVec3NormSqr(azaSubVec3(object->pos, object->target)) < azaSqr(0.1f)) {
			float angleMin = angleSize * (float)i;
			float angleMax = angleSize * (float)(i+1);
			float azimuth = randomf(angleMin, angleMax);
			float ac = cosf(azimuth), as = sinf(azimuth);
			float elevation = randomf(-AZA_TAU/4.0f, AZA_TAU/4.0f);
			float ec = cosf(elevation), es = sinf(elevation);
			float distance = sqrtf(randomf(0.0f, 1.0f));
			object->target = (azaVec3) {
				as * ec * distance * 10.0f,
				es * distance * 2.0f,
				ac * ec * distance * 5.0f,
			};
		}
		azaVec3 force = azaVec3NormalizedDef(azaSubVec3(object->target, object->pos), 0.001f, (azaVec3) { 0.0f, 1.0f, 0.0f });
		object->vel = azaAddVec3(object->vel, azaMulVec3Scalar(force, timeDelta * 1.0f));
		object->vel = azaMulVec3Scalar(object->vel, azaClampf(powf(2.0f, -timeDelta * 2.0f), 0.0f, 1.0f));
		object->posPrev = object->pos;
		object->pos = azaAddVec3(object->pos, azaMulVec3Scalar(object->vel, timeDelta));
	}
}

int catProcess(void *userdata, azaBuffer buffer) {
	float timeDelta = (float)buffer.frames / (float)buffer.samplerate;
	int err;
	updateObjects(bufferCat.channelLayout.count, timeDelta);
	azaBufferZero(buffer);
	azaBuffer sampledBuffer = azaPushSideBufferZero(buffer.frames, samplerCat->config.buffer->channelLayout.count, buffer.samplerate);

	if ((err = azaSamplerProcess(samplerCat, sampledBuffer))) {
		char buffer[64];
		AZA_LOG_ERR("azaSamplerProcess returned %s\n", azaErrorString(err, buffer, sizeof(buffer)));
		goto done;
	}

	for (uint8_t c = 0; c < bufferCat.channelLayout.count; c++) {
		float volumeStart = azaClampf(3.0f / azaVec3Norm(objects[c].posPrev), 0.0f, 1.0f);
		float volumeEnd = azaClampf(3.0f / azaVec3Norm(objects[c].pos), 0.0f, 1.0f);
		if ((err = azaSpatializeProcess(spatializeCat[c], buffer, azaBufferOneChannel(sampledBuffer, c), objects[c].posPrev, volumeStart, objects[c].pos, volumeEnd))) {
			char buffer[64];
			AZA_LOG_ERR("azaSpatializeProcess returned %s\n", azaErrorString(err, buffer, sizeof(buffer)));
			goto done;
		}
	}
done:
	azaPopSideBuffer();
	return err;
}

void usage(const char *executableName) {
	printf(
		"Usage:\n"
		"%s                      Listen to a purring cat move around\n"
		"%s --help               Display this help\n"
		"%s path/to/sound.ogg    Play the given sound file\n"
	, executableName, executableName, executableName);
}

int main(int argumentCount, char** argumentValues) {
	const char *soundFilename = "data/cat purring loop.ogg";

	if (argumentCount >= 2) {
		if (strcmp(argumentValues[1], "--help") == 0) {
			usage(argumentValues[0]);
			return 0;
		} else {
			soundFilename = argumentValues[1];
		}
	}

	if (loadSoundFileIntoBuffer(&bufferCat, soundFilename)) return 1;
	if (bufferCat.channelLayout.count == 0) {
		fprintf(stderr, "Sound \"%s\" has no channels!\n", soundFilename);
		return 1;
	}

	int err = azaInit();
	if (err) {
		char buffer[64];
		fprintf(stderr, "Failed to azaInit (%s)\n", azaErrorString(err, buffer, sizeof(buffer)));
		return 1;
	}

	if ((err = azaMixerStreamOpen(&mixer, (azaMixerConfig) { .trackCount = 2 } , (azaStreamConfig) {0}, false))) {
		char buffer[64];
		fprintf(stderr, "Failed to azaMixerStreamOpen (%s)\n", azaErrorString(err, buffer, sizeof(buffer)));
		return 1;
	}
	uint8_t outputChannelCount = azaStreamGetChannelLayout(&mixer.stream).count;

	// Configure all the DSP functions

	// Track 0

	azaDSPUserInitSingle(&dspSynth, sizeof(dspSynth), NULL, synthProcess);

	azaTrackAppendDSP(&mixer.tracks[0], (azaDSP*)&dspSynth);

	filter = azaMakeFilter((azaFilterConfig) {
		.kind = AZA_FILTER_LOW_PASS,
		.frequency = 200.0f,
	}, outputChannelCount);

	azaTrackAppendDSP(&mixer.tracks[0], (azaDSP*)filter);

	// We can use this to change the gain on an existing connection.
	azaTrackConnect(&mixer.tracks[0], &mixer.output, -9.0f);

	// Track 1

	azaDSPUserInitSingle(&dspCat, sizeof(dspCat), NULL, catProcess);

	samplerCat = azaMakeSampler((azaSamplerConfig) {
		.buffer = &bufferCat,
		.speed = 1.0f,
		.gain = 0.0f,
	});

	objects = calloc(bufferCat.channelLayout.count, sizeof(Object));
	updateObjects(bufferCat.channelLayout.count, 0.0f);
	spatializeCat = malloc(sizeof(azaSpatialize*) * bufferCat.channelLayout.count);
	for (uint8_t c = 0; c < bufferCat.channelLayout.count; c++) {
		objects[c].pos = objects[c].target;
		spatializeCat[c] = azaMakeSpatialize((azaSpatializeConfig) {
			.world       = AZA_WORLD_DEFAULT,
			.mode        = AZA_SPATIALIZE_ADVANCED,
			.delayMax    = 0.0f,
			.earDistance = 0.0f,
		}, outputChannelCount);
	}

	azaTrackAppendDSP(&mixer.tracks[1], (azaDSP*)&dspCat);

	// Master

	limiter = azaMakeLookaheadLimiter((azaLookaheadLimiterConfig) {
		.gainInput  = -3.0f,
		.gainOutput = -0.1f,
	}, outputChannelCount);

	azaTrackAppendDSP(&mixer.output, (azaDSP*)limiter);

	// Uncomment this to test if cyclic routing is detected
	// azaTrackConnect(&mixer.output, &mixer.tracks[0], 0.0f);

	azaMixerStreamSetActive(&mixer, true);

	// TODO: Make controls for the mixer
	printf("Press ENTER to stop\n");
	getc(stdin);
	azaMixerStreamClose(&mixer, false);

	free(objects);
	azaFreeSampler(samplerCat);
	for (uint8_t c = 0; c < bufferCat.channelLayout.count; c++) {
		azaFreeSpatialize(spatializeCat[c]);
	}
	free(spatializeCat);
	azaFreeLookaheadLimiter(limiter);
	azaBufferDeinit(&bufferCat);

	azaDeinit();
	return 0;
}

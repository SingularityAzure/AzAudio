/*
	File: audio.c
	Author: singularity
*/

#include "audio.h"

#include <spa/param/audio/format-utils.h>

#include <pipewire/pipewire.h>

#include <assert.h>
#include <stdlib.h>

#include "helpers.h"

#ifndef AZAUDIO_NO_STDIO
#include <stdio.h>
#else
#ifndef NULL
#define NULL 0
#endif
#endif

// Helper functions

int azaError;

azafpLogCallback azaPrint;

struct pw_thread_loop *loop;

struct {
	const struct spa_pod *params[1];
	uint8_t buffer[1024];
	struct spa_pod_builder builder;
} formatParams;

typedef struct {
	struct pw_stream *stream;
	struct pw_stream_events stream_events;
} azaStreamData;

static void azaStreamProcess(void *userdata) {
	azaStream *stream = userdata;
	azaStreamData *data = stream->data;
	struct pw_buffer *pw_buffer;
	struct spa_buffer *buffer;
	
	pw_buffer = pw_stream_dequeue_buffer(data->stream);
	if (pw_buffer == NULL) return;
	
	buffer = pw_buffer->buffer;
	assert(buffer->n_datas == 1);
	float *pcm = buffer->datas[0].data;
	if (pcm == NULL) return;
	int stride = sizeof(*pcm) * AZA_CHANNELS;
	int numFrames = buffer->datas[0].maxsize / stride;
	if (pw_buffer->requested) numFrames = SPA_MIN(pw_buffer->requested, numFrames);
	
	if (stream->capture) {
		stream->mixCallback(pcm, NULL, numFrames, AZA_CHANNELS, stream);
	} else {
		stream->mixCallback(NULL, pcm, numFrames, AZA_CHANNELS, stream);
	}
	
	buffer->datas[0].chunk->offset = 0;
	buffer->datas[0].chunk->stride = stride;
	buffer->datas[0].chunk->size = numFrames * stride;
	
	pw_stream_queue_buffer(data->stream, pw_buffer);
}

int azaInit() {
	azaPrint = azaDefaultLogFunc;
	
	formatParams.builder = SPA_POD_BUILDER_INIT(formatParams.buffer, sizeof(formatParams.buffer));
	
	formatParams.params[0] = spa_format_audio_raw_build(
		&formatParams.builder,
		SPA_PARAM_EnumFormat,
		&SPA_AUDIO_INFO_RAW_INIT(
			.format = SPA_AUDIO_FORMAT_F32,
			.channels = AZA_CHANNELS,
			.rate = AZA_SAMPLERATE
		)
	);
	
	int zero = 0;
	pw_init(&zero, NULL);
	
	loop = pw_thread_loop_new("AzAudio", NULL);
	pw_thread_loop_start(loop);
	
	azaError = AZA_SUCCESS;
	return azaError;
}

int azaClean() {
	pw_thread_loop_stop(loop);
	pw_thread_loop_destroy(loop);
	pw_deinit();
	azaError = AZA_SUCCESS;
	return azaError;
}

int azaInitStream(azaStream *stream, const char *device, int capture, azafpMixCallback mixCallback) {
	azaStreamData *data = calloc(sizeof(azaStreamData), 1);
	data->stream_events.version = PW_VERSION_STREAM_EVENTS;
	data->stream_events.process = azaStreamProcess;
	pw_thread_loop_lock(loop);
	data->stream = pw_stream_new_simple(
		pw_thread_loop_get_loop(loop),
		capture ? "AzAudio Capture" : "AzAudio Playback",
		pw_properties_new(
			PW_KEY_MEDIA_TYPE, "Audio",
			PW_KEY_MEDIA_CATEGORY, capture ? "Capture" : "Playback",
			PW_KEY_MEDIA_ROLE, "Music",
			NULL
		),
		&data->stream_events,
		stream
	);
	pw_stream_connect(
		data->stream,
		capture ? PW_DIRECTION_INPUT : PW_DIRECTION_OUTPUT,
		PW_ID_ANY,
		PW_STREAM_FLAG_AUTOCONNECT
		| PW_STREAM_FLAG_MAP_BUFFERS,
		// | PW_STREAM_FLAG_RT_PROCESS,
		formatParams.params, 1
	);
	pw_thread_loop_unlock(loop);
	stream->data = data;
	stream->capture = capture;
	stream->mixCallback = mixCallback;
	stream->sampleRate = AZA_SAMPLERATE;
	return AZA_SUCCESS;
}

void azaDeinitStream(azaStream *stream) {
	azaStreamData *data = stream->data;
	pw_thread_loop_lock(loop);
	pw_stream_disconnect(data->stream);
	pw_stream_destroy(data->stream);
	pw_thread_loop_unlock(loop);
}

int azaGetError() {
	return azaError;
}

void azaDefaultLogFunc(const char* message) {
	#ifndef AZAUDIO_NO_STDIO
	printf("AzAudio: %s\n",message);
	#endif
}

int azaSetLogCallback(azafpLogCallback newLogFunc) {
	if (newLogFunc != NULL) {
		azaPrint = newLogFunc;
	} else {
		azaPrint = azaDefaultLogFunc;
	}
	azaError = AZA_SUCCESS;
	return azaError;
}

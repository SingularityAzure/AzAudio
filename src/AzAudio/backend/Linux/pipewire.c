/*
	File: pipewire.c
	Author: Philip Haynes
*/

#include "../backend.h"
#include "../interface.h"
#include "../../error.h"
#include "../../AzAudio.h"
#include "../../helpers.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <assert.h>

#include <spa/param/audio/format-utils.h>
#include <pipewire/pipewire.h>

static void *pipewireSO;


// Bindings


static void
(*fp_pw_init)(int *argc, char **argv[]);

static void
(*fp_pw_deinit)(void);

static struct pw_thread_loop *
(*fp_pw_thread_loop_new)(const char *name, const struct spa_dict *props);

static void
(*fp_pw_thread_loop_destroy)(struct pw_thread_loop *loop);

static int
(*fp_pw_thread_loop_start)(struct pw_thread_loop *loop);

static void
(*fp_pw_thread_loop_stop)(struct pw_thread_loop *loop);

static void
(*fp_pw_thread_loop_lock)(struct pw_thread_loop *loop);

static void
(*fp_pw_thread_loop_unlock)(struct pw_thread_loop *loop);

static struct pw_loop *
(*fp_pw_thread_loop_get_loop)(struct pw_thread_loop *loop);

static struct pw_stream *
(*fp_pw_stream_new_simple)(struct pw_loop *loop, const char *name, struct pw_properties *props, const struct pw_stream_events *events, void *data);

static void
(*fp_pw_stream_destroy)(struct pw_stream *stream);

static int
(*fp_pw_stream_connect)(struct pw_stream *stream, enum pw_direction direction, uint32_t target_id, enum pw_stream_flags flags, const struct spa_pod **params, uint32_t n_params);

static int
(*fp_pw_stream_disconnect)(struct pw_stream *stream);

static struct pw_properties *
(*fp_pw_properties_new)(const char *key, ...) SPA_SENTINEL;

static struct pw_buffer *
(*fp_pw_stream_dequeue_buffer)(struct pw_stream *stream);

static int
(*fp_pw_stream_queue_buffer)(struct pw_stream *stream, struct pw_buffer *buffer);



static struct pw_thread_loop *loop;

static struct {
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
	
	pw_buffer = fp_pw_stream_dequeue_buffer(data->stream);
	if (pw_buffer == NULL) return;
	
	buffer = pw_buffer->buffer;
	assert(buffer->n_datas == 1);
	float *pcm = buffer->datas[0].data;
	if (pcm == NULL) return;
	int stride = sizeof(*pcm) * stream->channels;
	int numFrames = buffer->datas[0].chunk->size / stride;
	if (pw_buffer->requested) numFrames = SPA_MAX(pw_buffer->requested, numFrames);
	
	stream->mixCallback((azaBuffer){
		.samples = pcm,
		.frames = numFrames,
		.stride = stream->channels,
		.channels = stream->channels,
		.samplerate = stream->samplerate,
	}, stream->userdata);
	
	// printf("%d frames.\n", numFrames);
	
	buffer->datas[0].chunk->offset = 0;
	buffer->datas[0].chunk->stride = stride;
	buffer->datas[0].chunk->size = numFrames * stride;
	
	fp_pw_stream_queue_buffer(data->stream, pw_buffer);
}

static int azaPipewireInit() {
	formatParams.builder = SPA_POD_BUILDER_INIT(formatParams.buffer, sizeof(formatParams.buffer));
	
	// TODO: query device for channels and samplerate
	formatParams.params[0] = spa_format_audio_raw_build(
		&formatParams.builder,
		SPA_PARAM_EnumFormat,
		&SPA_AUDIO_INFO_RAW_INIT(
			.format = SPA_AUDIO_FORMAT_F32,
			.channels = AZA_CHANNELS_DEFAULT,
			.rate = AZA_SAMPLERATE_DEFAULT
		)
	);
	
	int zero = 0;
	fp_pw_init(&zero, NULL);
	
	loop = fp_pw_thread_loop_new("AzAudio", NULL);
	fp_pw_thread_loop_start(loop);
	return AZA_SUCCESS;
}

static int azaPipewireDeinit() {
	fp_pw_thread_loop_stop(loop);
	fp_pw_thread_loop_destroy(loop);
	fp_pw_deinit();
	return AZA_SUCCESS;
}

static int azaStreamInitPipewire(azaStream *stream, const char *device) {
	if (stream->mixCallback == NULL) {
		AZA_PRINT_ERR("azaStreamInitPipewire error: no mix callback provided.\n");
		return AZA_ERROR_NULL_POINTER;
	}
	azaStreamData *data = calloc(sizeof(azaStreamData), 1);
	data->stream_events.version = PW_VERSION_STREAM_EVENTS;
	data->stream_events.process = azaStreamProcess;
	fp_pw_thread_loop_lock(loop);
	const char *streamName;
	const char *streamMediaCategory;
	enum spa_direction streamSpaDirection;
	switch (stream->deviceInterface) {
		case AZA_OUTPUT:
			streamName = "AzAudio Playback";
			streamMediaCategory = "Playback";
			streamSpaDirection = PW_DIRECTION_OUTPUT;
			break;
		case AZA_INPUT:
			streamName = "AzAudio Capture";
			streamMediaCategory = "Capture";
			streamSpaDirection = PW_DIRECTION_INPUT;
			break;
		default:
			AZA_PRINT_ERR("azaInitStream error: stream->deviceInterface (%d) is invalid.\n", stream->deviceInterface);
			return AZA_ERROR_INVALID_CONFIGURATION;
			break;
	}
	data->stream = fp_pw_stream_new_simple(
		fp_pw_thread_loop_get_loop(loop),
		streamName,
		fp_pw_properties_new(
			PW_KEY_MEDIA_TYPE, "Audio",
			PW_KEY_MEDIA_CATEGORY, streamMediaCategory,
			PW_KEY_MEDIA_ROLE, "Game",
			NULL
		),
		&data->stream_events,
		stream
	);
	fp_pw_stream_connect(
		data->stream,
		streamSpaDirection,
		PW_ID_ANY,
		PW_STREAM_FLAG_AUTOCONNECT
		| PW_STREAM_FLAG_MAP_BUFFERS,
		// | PW_STREAM_FLAG_RT_PROCESS,
		formatParams.params, 1
	);
	fp_pw_thread_loop_unlock(loop);
	stream->data = data;
	// TODO: Query device
	if (stream->channels == 0)
		stream->channels = AZA_CHANNELS_DEFAULT;
	if (stream->samplerate == 0)
		stream->samplerate = AZA_SAMPLERATE_DEFAULT;
	return AZA_SUCCESS;
}

static void azaStreamDeinitPipewire(azaStream *stream) {
	azaStreamData *data = stream->data;
	fp_pw_thread_loop_lock(loop);
	fp_pw_stream_disconnect(data->stream);
	fp_pw_stream_destroy(data->stream);
	fp_pw_thread_loop_unlock(loop);
	free(data);
}


#define BIND_SYMBOL(symname) \
fp_ ## symname = dlsym(pipewireSO, #symname);\
if ((err = dlerror())) return AZA_ERROR_BACKEND_ERROR

int azaBackendPipewireInit() {
	char *err;
	pipewireSO = dlopen("libpipewire-0.3.so", RTLD_LAZY);
	if (!pipewireSO) {
		return AZA_ERROR_BACKEND_UNAVAILABLE;
	}
	dlerror();
	BIND_SYMBOL(pw_init);
	BIND_SYMBOL(pw_deinit);
	BIND_SYMBOL(pw_thread_loop_new);
	BIND_SYMBOL(pw_thread_loop_destroy);
	BIND_SYMBOL(pw_thread_loop_start);
	BIND_SYMBOL(pw_thread_loop_stop);
	BIND_SYMBOL(pw_thread_loop_lock);
	BIND_SYMBOL(pw_thread_loop_unlock);
	BIND_SYMBOL(pw_thread_loop_get_loop);
	BIND_SYMBOL(pw_stream_new_simple);
	BIND_SYMBOL(pw_stream_destroy);
	BIND_SYMBOL(pw_stream_connect);
	BIND_SYMBOL(pw_stream_disconnect);
	BIND_SYMBOL(pw_properties_new);
	BIND_SYMBOL(pw_stream_dequeue_buffer);
	BIND_SYMBOL(pw_stream_queue_buffer);
	
	azaStreamInit = azaStreamInitPipewire;
	azaStreamDeinit = azaStreamDeinitPipewire;
	
	return azaPipewireInit();
}

void azaBackendPipewireDeinit() {
	azaPipewireDeinit();
	dlclose(pipewireSO);
}
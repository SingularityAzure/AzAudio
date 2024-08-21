/*
	File: pipewire.c
	Author: Philip Haynes
*/

#include "../backend.h"
#include "../interface.h"
#include "../../error.h"
#include "../../helpers.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <assert.h>

#include <spa/param/audio/format-utils.h>
#include <pipewire/pipewire.h>

#include <threads.h>

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

struct pw_context *
(*fp_pw_context_new)(struct pw_loop *main_loop, struct pw_properties *props, size_t user_data_size);

static void
(*fp_pw_context_destroy)(struct pw_context *context);

static struct pw_core *
(*fp_pw_context_connect)(struct pw_context *context, struct pw_properties *properties, size_t user_data_size);

static int
(*fp_pw_core_disconnect)(struct pw_core *core);

static void
(*fp_pw_proxy_destroy)(struct pw_proxy *proxy);



static struct pw_thread_loop *loop;
static struct pw_context *context;
static struct pw_core *core;
static struct pw_registry *registry;
static struct spa_hook registry_listener;

static int flushSeq;
static int flushSynced = AZA_FALSE;

static void azaCoreDone(void *data, uint32_t id, int seq) {
	if (id == PW_ID_CORE && seq == flushSeq) {
		flushSynced = AZA_TRUE;
	}
}

static void azaPipewireFlush() {
	static const struct pw_core_events core_events = {
		PW_VERSION_CORE,
		.done = azaCoreDone,
	};
	struct spa_hook core_listener;

	fp_pw_thread_loop_lock(loop);
	flushSynced = AZA_FALSE;

	pw_core_add_listener(core, &core_listener, &core_events, NULL);
	flushSeq = pw_core_sync(core, PW_ID_CORE, 0);

	do {
		fp_pw_thread_loop_unlock(loop);
		struct timespec timespec = {
			.tv_nsec = 1000,
			.tv_sec = 0,
		};
		thrd_sleep(&timespec, NULL);
		fp_pw_thread_loop_lock(loop);
	} while (!flushSynced);

	spa_hook_remove(&core_listener);

	fp_pw_thread_loop_unlock(loop);
}

// This is meant to be called in the thread_loop thread. This allows you to flush future events.
static void azaPipewireRefreshFlush() {
	flushSeq = pw_core_sync(core, PW_ID_CORE, 0);
}

/*
static struct pw_port *port[128];
static struct spa_hook port_listener[128];
static size_t next_port = 0;

static void azaPortInfo(void *data, const struct pw_port_info *info) {
	const struct spa_dict_item *item;

	AZA_LOG_INFO("port: id:%u\n", info->id);
	AZA_LOG_INFO("\tprops:\n");
	spa_dict_for_each(item, info->props) {
		AZA_LOG_INFO("\t\t%s: \"%s\"\n", item->key, item->value);
	}
}

static const struct pw_port_events port_events = {
	PW_VERSION_PORT_EVENTS,
	.info = azaPortInfo,
};
*/

#define AZA_MAX_NODES 256

static struct pw_node *node[AZA_MAX_NODES];
static struct spa_hook node_listener[AZA_MAX_NODES];
static size_t next_node = 0;

struct azaNodeInfo {
	// Device name
	const char *node_name;
	// Human-readable name
	const char *node_description;
	// Short, human-readable name
	const char *node_nick;
	// Comma-separated list of channel positions
	const char *audio_position;
	// How many channels this node uses
	unsigned audio_channels;
	// priority for being chosen (higher is more preferred)
	int priority_session;
	uint32_t object_id;
	const char *object_serial;
};

static struct azaNodeInfo nodeOutput[AZA_MAX_NODES];
static size_t nodeOutputCount = 0;
static struct azaNodeInfo nodeInput[AZA_MAX_NODES];
static size_t nodeInputCount = 0;

static void azaNodeEmplace(struct azaNodeInfo arr[], size_t *count, struct azaNodeInfo nodeInfo) {
	for (size_t i = 0; i < *count; i++) {
		struct azaNodeInfo *node = &arr[i];
		if (node->object_id == nodeInfo.object_id) {
			*node = nodeInfo;
			return;
		}
	}
	if (*count >= AZA_MAX_NODES) {
		AZA_LOG_ERR("Tried to emplace a new azaNodeInfo but ran out of space for more!\n");
		return;
	}
	arr[*count] = nodeInfo;
	(*count)++;
}

static void azaNodeInfo(void *data, const struct pw_node_info *info) {
	const struct spa_dict_item *item;
	struct azaNodeInfo nodeInfo;
	int isOutput = AZA_FALSE, isInput = AZA_FALSE;
	nodeInfo.object_id = info->id;
	AZA_LOG_TRACE("node: id:%u\n", info->id);
	AZA_LOG_TRACE("\tprops:\n");
	spa_dict_for_each(item, info->props) {
		if (strcmp(item->key, PW_KEY_NODE_NAME) == 0) {
			nodeInfo.node_name = item->value;
		} else if (strcmp(item->key, PW_KEY_NODE_DESCRIPTION) == 0) {
			nodeInfo.node_description = item->value;
		} else if (strcmp(item->key, PW_KEY_NODE_NICK) == 0) {
			nodeInfo.node_nick = item->value;
		} else if (strcmp(item->key, "audio.position") == 0) {
			nodeInfo.audio_position = item->value;
		} else if (strcmp(item->key, PW_KEY_AUDIO_CHANNELS) == 0) {
			nodeInfo.audio_channels = atoi(item->value);
		} else if (strcmp(item->key, PW_KEY_PRIORITY_SESSION) == 0) {
			nodeInfo.priority_session = atoi(item->value);
		} else if (strcmp(item->key, PW_KEY_OBJECT_SERIAL) == 0) {
			nodeInfo.object_serial = item->value;
		} else if (strcmp(item->key, PW_KEY_MEDIA_CLASS) == 0) {
			if (strcmp(item->value, "Audio/Sink") == 0) {
				isOutput = AZA_TRUE;
			} else if (strcmp(item->value, "Audio/Source") == 0) {
				isInput = AZA_TRUE;
			}
		}
		AZA_LOG_TRACE("\t\t%s: \"%s\"\n", item->key, item->value);
	}

	if (isOutput) {
		// AZA_LOG_INFO("Found output device: \"%s\"\n", nodeInfo.node_description);
		azaNodeEmplace(nodeOutput, &nodeOutputCount, nodeInfo);
	} else if (isInput) {
		// AZA_LOG_INFO("Found input device: \"%s\"\n", nodeInfo.node_description);
		azaNodeEmplace(nodeInput, &nodeInputCount, nodeInfo);
	}
}

static const struct pw_node_events node_events = {
	PW_VERSION_NODE_EVENTS,
	.info = azaNodeInfo,
};

/*
static struct pw_device *device[128];
static struct spa_hook device_listener[128];
static size_t next_device = 0;

static void azaDeviceInfo(void *data, const struct pw_device_info *info) {
	const struct spa_dict_item *item;

	AZA_LOG_INFO("device: id:%u\n", info->id);
	AZA_LOG_INFO("\tprops:\n");
	spa_dict_for_each(item, info->props) {
		AZA_LOG_INFO("\t\t%s: \"%s\"\n", item->key, item->value);
	}
	AZA_LOG_INFO("\tparams:\n");
	for (uint32_t i = 0; i < info->n_params; i++) {
		AZA_LOG_INFO("\t\tid:%u flags:%x\n", info->params[i].id, info->params[i].flags);
	}
}

static const struct pw_device_events device_events = {
	PW_VERSION_DEVICE_EVENTS,
	.info = azaDeviceInfo,
};
*/

/*
static struct pw_client *clients[128];
static struct spa_hook client_listener[128];
static size_t next_client = 0;


static void azaClientInfo(void *data, const struct pw_client_info *info) {
	const struct spa_dict_item *item;

	AZA_LOG_INFO("client: id:%u\n", info->id);
	AZA_LOG_INFO("\tprops:\n");
	spa_dict_for_each(item, info->props) {
		AZA_LOG_INFO("\t\t%s: \"%s\"\n", item->key, item->value);
	}
}

static const struct pw_client_events client_events = {
	PW_VERSION_CLIENT_EVENTS,
	.info = azaClientInfo,
};
*/

static void azaRegistryEventGlobal(void *data, uint32_t id, uint32_t permissions, const char *type, uint32_t version, const struct spa_dict *props) {
	AZA_LOG_TRACE("object: id:%u type:%s/%d\n", id, type, version);
	int addedListener = AZA_FALSE;
	/*
	if (strcmp(type, PW_TYPE_INTERFACE_Client) == 0) {
		clients[next_client] = pw_registry_bind(registry, id, type, PW_VERSION_CLIENT, 0);
		pw_client_add_listener(clients[next_client], &client_listener[next_client], &client_events, NULL);
		next_client++;
	}
	*/
	/*
	if (strcmp(type, PW_TYPE_INTERFACE_Device) == 0) {
		device[next_device] = pw_registry_bind(registry, id, type, PW_VERSION_DEVICE, 0);
		pw_device_add_listener(device[next_device], &device_listener[next_device], &device_events, NULL);
		next_device++;
	}
	*/
	if (strcmp(type, PW_TYPE_INTERFACE_Node) == 0) {
		if (next_node == AZA_MAX_NODES) {
			AZA_LOG_ERR("Ran out of node slots!\n");
		} else {
			node[next_node] = pw_registry_bind(registry, id, type, PW_VERSION_NODE, 0);
			pw_node_add_listener(node[next_node], &node_listener[next_node], &node_events, NULL);
			next_node++;
			addedListener = AZA_TRUE;
		}
	}
	/*
	if (strcmp(type, PW_TYPE_INTERFACE_Port) == 0) {
		port[next_port] = pw_registry_bind(registry, id, type, PW_VERSION_PORT, 0);
		pw_port_add_listener(port[next_port], &port_listener[next_port], &port_events, NULL);
		next_port++;
	}
	*/
	if (addedListener) {
		azaPipewireRefreshFlush();
	}
}

static const struct pw_registry_events registry_events = {
	PW_VERSION_REGISTRY_EVENTS,
	.global = azaRegistryEventGlobal,
};

typedef struct azaSpaPod {
	const struct spa_pod *params[1];
	uint8_t buffer[1024];
	struct spa_pod_builder builder;
} azaSpaPod;

static void azaMakeSpaPodFormat(azaSpaPod *dst, enum spa_audio_format format, int channels, int samplerate) {
	dst->builder = SPA_POD_BUILDER_INIT(dst->buffer, sizeof(dst->buffer));

	dst->params[0] = spa_format_audio_raw_build(
		&dst->builder,
		SPA_PARAM_EnumFormat,
		&SPA_AUDIO_INFO_RAW_INIT(
			.format = format,
			.channels = channels,
			.rate = samplerate
		)
	);
}



typedef struct azaStreamData {
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

	buffer->datas[0].chunk->offset = 0;
	buffer->datas[0].chunk->stride = stride;
	buffer->datas[0].chunk->size = numFrames * stride;

	fp_pw_stream_queue_buffer(data->stream, pw_buffer);
}



static int azaPipewireInit() {
	int zero = 0;
	fp_pw_init(&zero, NULL);

	loop = fp_pw_thread_loop_new("AzAudio", NULL);

	context = fp_pw_context_new(fp_pw_thread_loop_get_loop(loop), NULL, 0);
	core = fp_pw_context_connect(context, NULL, 0);
	if (!core) {
		AZA_LOG_ERR("azaPipewireInit error: Failed to connect context\n");
		return AZA_ERROR_BACKEND_ERROR;
	}
	registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);

	spa_zero(registry_listener);
	pw_registry_add_listener(registry, &registry_listener, &registry_events, NULL);

	fp_pw_thread_loop_start(loop);

	azaPipewireFlush();
	return AZA_SUCCESS;
}

static int azaPipewireDeinit() {
	fp_pw_thread_loop_stop(loop);

	fp_pw_proxy_destroy((struct pw_proxy*)registry);
	fp_pw_core_disconnect(core);
	fp_pw_context_destroy(context);

	fp_pw_thread_loop_destroy(loop);
	fp_pw_deinit();
	return AZA_SUCCESS;
}

static int azaStreamInitPipewire(azaStream *stream, const char *device) {
	if (stream->mixCallback == NULL) {
		AZA_LOG_ERR("azaStreamInitPipewire error: no mix callback provided.\n");
		return AZA_ERROR_NULL_POINTER;
	}
	struct azaNodeInfo *deviceNodePool = NULL;
	size_t deviceNodeCount = 0;

	azaSpaPod formatPod;
	azaMakeSpaPodFormat(&formatPod, SPA_AUDIO_FORMAT_F32, stream->channels, stream->samplerate);

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
			deviceNodePool = nodeOutput;
			deviceNodeCount = nodeOutputCount;
			break;
		case AZA_INPUT:
			streamName = "AzAudio Capture";
			streamMediaCategory = "Capture";
			streamSpaDirection = PW_DIRECTION_INPUT;
			deviceNodePool = nodeInput;
			deviceNodeCount = nodeInputCount;
			break;
		default:
			AZA_LOG_ERR("azaStreamInitPipewire error: stream->deviceInterface (%d) is invalid.\n", stream->deviceInterface);
			return AZA_ERROR_INVALID_CONFIGURATION;
			break;
	}

	size_t channelsDefault = AZA_CHANNELS_DEFAULT;
	size_t samplerateDefault = AZA_SAMPLERATE_DEFAULT;

	struct azaNodeInfo *deviceNodeInfo = NULL;
	// Search the nodes for the device name
	for (size_t i = 0; i < deviceNodeCount; i++){
		struct azaNodeInfo *node = &deviceNodePool[i];
		if (strcmp(node->node_description, device) == 0) {
			deviceNodeInfo = node;
			AZA_LOG_INFO("Chose device by name: \"%s\"\n", device);
			break;
		}
	}
	if (!deviceNodeInfo) {
		// Find device with highest priority
		struct azaNodeInfo *bestNode = NULL;
		int highestPriority = INT32_MIN;
		for (size_t i = 0; i < deviceNodeCount; i++) {
			struct azaNodeInfo *node = &deviceNodePool[i];
			if (node->priority_session > highestPriority) {
				bestNode = node;
				highestPriority = node->priority_session;
			}
		}
		if (bestNode) {
			deviceNodeInfo = bestNode;
			AZA_LOG_INFO("Chose device by priority: \"%s\"\n", deviceNodeInfo->node_description);
		}
	}

	struct pw_properties *properties;
	if (deviceNodeInfo) {
		properties = fp_pw_properties_new(
			PW_KEY_MEDIA_TYPE, "Audio",
			PW_KEY_MEDIA_CATEGORY, streamMediaCategory,
			PW_KEY_MEDIA_ROLE, "Game",
			// NOTE: Either works, not sure if it matters at all
			// PW_KEY_TARGET_OBJECT, deviceNodeInfo->object_serial,
			PW_KEY_TARGET_OBJECT, deviceNodeInfo->node_name,
			NULL
		);
		channelsDefault = deviceNodeInfo->audio_channels;
	} else {
		AZA_LOG_INFO("Letting pipewire choose a device for us...\n");
		properties = fp_pw_properties_new(
			PW_KEY_MEDIA_TYPE, "Audio",
			PW_KEY_MEDIA_CATEGORY, streamMediaCategory,
			PW_KEY_MEDIA_ROLE, "Game",
			NULL
		);
	}

	if (stream->channels == 0)
		stream->channels = channelsDefault;
	if (stream->samplerate == 0)
		stream->samplerate = samplerateDefault;

	data->stream = fp_pw_stream_new_simple(
		fp_pw_thread_loop_get_loop(loop),
		streamName,
		properties,
		&data->stream_events,
		stream
	);
	fp_pw_stream_connect(
		data->stream,
		streamSpaDirection,
		PW_ID_ANY,
		PW_STREAM_FLAG_AUTOCONNECT
		| PW_STREAM_FLAG_MAP_BUFFERS
		// | PW_STREAM_FLAG_RT_PROCESS
		,
		formatPod.params, 1
	);
	fp_pw_thread_loop_unlock(loop);
	stream->data = data;
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

static size_t azaGetDeviceCountPipewire(azaDeviceInterface interface) {
	switch (interface) {
		case AZA_OUTPUT: return nodeOutputCount;
		case AZA_INPUT: return nodeInputCount;
		default: return 0;
	}
}

static const char* azaGetDeviceNamePipewire(azaDeviceInterface interface, size_t index) {
	switch (interface) {
		case AZA_OUTPUT:
			assert(index < nodeOutputCount);
			return nodeOutput[index].node_description;
			break;
		case AZA_INPUT:
			assert(index < nodeInputCount);
			return nodeInput[index].node_description;
			break;
		default: return 0;
	}
}

static size_t azaGetDeviceChannelsPipewire(azaDeviceInterface interface, size_t index) {
	switch (interface) {
		case AZA_OUTPUT:
			assert(index < nodeOutputCount);
			return nodeOutput[index].audio_channels;
			break;
		case AZA_INPUT:
			assert(index < nodeInputCount);
			return nodeInput[index].audio_channels;
			break;
		default: return 0;
	}
}


#define BIND_SYMBOL(symname) \
fp_ ## symname = dlsym(pipewireSO, #symname);\
if ((err = dlerror())) return AZA_ERROR_BACKEND_LOAD_ERROR

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
	BIND_SYMBOL(pw_context_new);
	BIND_SYMBOL(pw_context_destroy);
	BIND_SYMBOL(pw_context_connect);
	BIND_SYMBOL(pw_core_disconnect);
	BIND_SYMBOL(pw_proxy_destroy);

	azaStreamInit = azaStreamInitPipewire;
	azaStreamDeinit = azaStreamDeinitPipewire;
	azaGetDeviceCount = azaGetDeviceCountPipewire;
	azaGetDeviceName = azaGetDeviceNamePipewire;
	azaGetDeviceChannels = azaGetDeviceChannelsPipewire;

	return azaPipewireInit();
}

void azaBackendPipewireDeinit() {
	azaPipewireDeinit();
	dlclose(pipewireSO);
}
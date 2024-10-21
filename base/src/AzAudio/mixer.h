/*
	File: mixer.h
	Author: Philip Haynes
	General purpose mixer with track routing and DSP plugins.
*/

#ifndef AZAUDIO_MIXING_H
#define AZAUDIO_MIXING_H

#include "dsp.h"
#include "backend/interface.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct azaTrackRoute {
	struct azaTrack *track;
	float gain;
} azaTrackRoute;

// a track has the capabilities of a bus and can have sound sources on it
typedef struct azaTrack {
	azaBuffer buffer;
	// Plugin chain, including synths and samplers
	azaDSP *dsp;
	struct {
		azaTrackRoute *data;
		uint32_t count;
		uint32_t capacity;
	} receives;
	// Used to determine whether routing is cyclic.
	uint8_t mark;
} azaTrack;
// Initializes our buffer
// May return any error azaBufferInit can return
int azaTrackInit(azaTrack *data, uint32_t bufferFrames, azaChannelLayout bufferChannelLayout);
void azaTrackDeinit(azaTrack *data);
// Adds a dsp to the end of the dsp chain
void azaTrackAppendDSP(azaTrack *data, azaDSP *dsp);
// Adds a dsp to the beginning of the dsp chain
void azaTrackPrependDSP(azaTrack *data, azaDSP *dsp);

// Routes the output of from to the input of to and returns the connection
azaTrackRoute* azaTrackConnect(azaTrack *from, azaTrack *to, float gain);
void azaTrackDisconnect(azaTrack *from, azaTrack *to);

int azaTrackProcess(uint32_t frames, uint32_t samplerate, azaTrack *data);

typedef struct azaMixerConfig {
	uint32_t trackCount;
	uint32_t bufferFrames;
} azaMixerConfig;

typedef struct azaMixer {
	azaMixerConfig config;
	azaTrack *tracks;
	azaTrack output;
	// We may optionally own a stream to which we output the track contents of output.
	azaStream stream;
} azaMixer;

// Allocates config.trackCount tracks and initializes them
// If config.isOutputRemote is zero, also initializes the inline output track
// bufferFrames indicates how many frames our buffers should have. This should probably match the maximum size of the backend buffer.
// bufferChannelLayout will be used to initialize buffer channel layouts
// May return AZA_ERROR_OUT_OF_MEMORY if we failed to allocate tracks, or any error azaBufferInit can return
int azaMixerInit(azaMixer *data, azaMixerConfig config, azaChannelLayout bufferChannelLayout);
void azaMixerDeinit(azaMixer *data);
// Processes all the tracks to produce a result into the output track.
// frames MUST be <= data->config.bufferFrames
int azaMixerProcess(uint32_t frames, uint32_t samplerate, azaMixer *data);

// Builtin callback for processing the mixer on a stream
int azaMixerCallback(void *userdata, azaBuffer buffer);

// Opens an output stream to process this mixer and initializes it such that the tracks have enough frames.
// config.bufferFrames is set to the max of the value passed in or the number required for the output stream. As such you can leave this at zero.
// if activate is true then this call will also start the stream immediately without you needing to call azaMixerStreamSetActive. Passing false into this helps if you want to configure DSP based on unknown device factors, such as if you let the device choose the samplerate and channel count.
int azaMixerStreamOpen(azaMixer *data, azaMixerConfig config, azaStreamConfig streamConfig, bool activate);

// if preserveMixer is false, then we also call azaMixerDeinit.
void azaMixerStreamClose(azaMixer *data, bool preserveMixer);

static inline void azaMixerStreamSetActive(azaMixer *data, bool active) {
	azaStreamSetActive(&data->stream, active);
}

#ifdef __cplusplus
}
#endif

#endif // AZAUDIO_MIXING_H
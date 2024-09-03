/*
	File: interface.c
	Author: Philip Haynes
*/

#include "interface.h"

#include "../helpers.h"
#include "backend.h"
#include "../error.h"

typedef enum azaBackend {
	AZA_BACKEND_NONE=0,
#ifdef __unix
	AZA_BACKEND_PIPEWIRE,
	AZA_BACKEND_PULSEAUDIO,
	AZA_BACKEND_JACK,
	AZA_BACKEND_ALSA,
#elif defined(_WIN32)
	AZA_BACKEND_WASAPI,
	AZA_BACKEND_XAUDIO2,
#endif
} azaBackend;
static azaBackend backend = AZA_BACKEND_NONE;

int azaBackendInit() {
	if (0) {
#ifdef __unix
	} else if (AZA_SUCCESS == azaBackendPipewireInit()) {
		backend = AZA_BACKEND_PIPEWIRE;
		AZA_LOG_INFO("AzAudio will use backend \"Pipewire\"\n");
	} else if (AZA_SUCCESS == azaBackendPulseAudioInit()) {
		backend = AZA_BACKEND_PULSEAUDIO;
		AZA_LOG_INFO("AzAudio will use backend \"PulseAudio\"\n");
	} else if (AZA_SUCCESS == azaBackendJackInit()) {
		backend = AZA_BACKEND_JACK;
		AZA_LOG_INFO("AzAudio will use backend \"Jack\"\n");
	} else if (AZA_SUCCESS == azaBackendALSAInit()) {
		backend = AZA_BACKEND_ALSA;
		AZA_LOG_INFO("AzAudio will use backend \"ALSA\"\n");
#elif defined(_WIN32)
	} else if (AZA_SUCCESS == azaBackendWASAPIInit()) {
		backend = AZA_BACKEND_WASAPI;
		AZA_LOG_INFO("AzAudio will use backend \"WASAPI\"\n");
	} else if (AZA_SUCCESS == azaBackendXAudio2Init()) {
		backend = AZA_BACKEND_XAUDIO2;
		AZA_LOG_INFO("AzAudio will use backend \"XAudio2\"\n");
#endif
	} else {
		AZA_LOG_ERR("No backends available :(\n");
		return AZA_ERROR_BACKEND_UNAVAILABLE;
	}
	return AZA_SUCCESS;
}

void azaBackendDeinit() {
	switch (backend) {
#ifdef __unix
		case AZA_BACKEND_PIPEWIRE:
			azaBackendPipewireDeinit();
			break;
		case AZA_BACKEND_PULSEAUDIO:
			azaBackendPulseAudioDeinit();
			break;
		case AZA_BACKEND_JACK:
			azaBackendJackDeinit();
			break;
		case AZA_BACKEND_ALSA:
			azaBackendALSADeinit();
			break;
#elif defined(_WIN32)
		case AZA_BACKEND_WASAPI:
			azaBackendWASAPIDeinit();
			break;
		case AZA_BACKEND_XAUDIO2:
			azaBackendXAudio2Deinit();
			break;
#endif
		default: break;
	}
}

fp_azaStreamInit azaStreamInit;
fp_azaStreamDeinit azaStreamDeinit;
fp_azaStreamGetDeviceName azaStreamGetDeviceName;
fp_azaStreamGetSamplerate azaStreamGetSamplerate;
fp_azaStreamGetChannels azaStreamGetChannels;
fp_azaGetDeviceCount azaGetDeviceCount;
fp_azaGetDeviceName azaGetDeviceName;
fp_azaGetDeviceChannels azaGetDeviceChannels;
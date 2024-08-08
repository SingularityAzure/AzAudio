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
	AZA_BACKEND_WINTENDO,
#endif
} azaBackend;
static azaBackend backend = AZA_BACKEND_NONE;

int azaBackendInit() {
	if (0) {
#ifdef __unix
	} else if (AZA_SUCCESS == azaBackendPipewireInit()) {
		backend = AZA_BACKEND_PIPEWIRE;
		AZA_PRINT_INFO("AzAudio will use backend \"Pipewire\"\n");
	} else if (AZA_SUCCESS == azaBackendPulseAudioInit()) {
		backend = AZA_BACKEND_PULSEAUDIO;
		AZA_PRINT_INFO("AzAudio will use backend \"PulseAudio\"\n");
	} else if (AZA_SUCCESS == azaBackendJackInit()) {
		backend = AZA_BACKEND_JACK;
		AZA_PRINT_INFO("AzAudio will use backend \"Jack\"\n");
	} else if (AZA_SUCCESS == azaBackendALSAInit()) {
		backend = AZA_BACKEND_ALSA;
		AZA_PRINT_INFO("AzAudio will use backend \"ALSA\"\n");
#elif defined(_WIN32)
	} else if (AZA_SUCCESS == azaBackendWintendoInit()) {
		backend = AZA_BACKEND_WINTENDO;
		AZA_PRINT_INFO("AzAudio will use backend \"Wintendo >.>\"\n");
#endif
	} else {
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
		case AZA_BACKEND_WINTENDO:
			azaBackendWintendoDeinit();
			break;
#endif
		default: break;
	}
}

fp_azaStreamInit azaStreamInit;
fp_azaStreamDeinit azaStreamDeinit;
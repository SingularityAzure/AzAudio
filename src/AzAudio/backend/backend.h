/*
	File: backend.h
	Author: Philip Haynes
	Functions that try to setup backends.
*/

#ifndef AZAUDIO_BACKEND_H
#define AZAUDIO_BACKEND_H

#ifdef __unix

// TODO: Some of these will be stubs that return 0 until their backends get implemented.

int azaBackendPipewireInit();
void azaBackendPipewireDeinit();

int azaBackendPulseAudioInit();
void azaBackendPulseAudioDeinit();

int azaBackendJackInit();
void azaBackendJackDeinit();

int azaBackendALSAInit();
void azaBackendALSADeinit();

#elif defined(_WIN32)

// TODO: Figure out what Windows backends make sense
int azaBackendWintendoInit();
void azaBackendWintendoDeinit();

#endif

#endif // AZAUDIO_BACKEND_H
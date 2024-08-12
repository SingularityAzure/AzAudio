/*
	File: backend.h
	Author: Philip Haynes
	Functions that try to setup backends.
*/

#ifndef AZAUDIO_BACKEND_H
#define AZAUDIO_BACKEND_H

// TODO: Some of these will be stubs that return 0 until their backends get implemented.

#ifdef __unix

int azaBackendPipewireInit();
void azaBackendPipewireDeinit();

int azaBackendPulseAudioInit();
void azaBackendPulseAudioDeinit();

int azaBackendJackInit();
void azaBackendJackDeinit();

int azaBackendALSAInit();
void azaBackendALSADeinit();

#elif defined(_WIN32)

int azaBackendWASAPIInit();
void azaBackendWASAPIDeinit();

// TODO: Implement XAudio2 backend
int azaBackendXAudio2Init();
void azaBackendXAudio2Deinit();

#endif

#endif // AZAUDIO_BACKEND_H
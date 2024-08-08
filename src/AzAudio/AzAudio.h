/*
	File: AzAudio.h
	Author: Philip Haynes
	Main entry point to using the AzAudio library.
*/

// #define SYS_AUDIO_NO_STDIO // to remove stdio dependency

#ifndef AZAUDIO_H
#define AZAUDIO_H

#include "backend/interface.h"

#ifdef __cplusplus
extern "C" {
#endif

// Defaults in case querying the devices doesn't work.

#ifndef AZA_SAMPLERATE_DEFAULT
#define AZA_SAMPLERATE_DEFAULT 48000
#endif

#ifndef AZA_CHANNELS_DEFAULT
#define AZA_CHANNELS_DEFAULT 2
#endif

#define AZA_TRUE 1
#define AZA_FALSE 0

// Setup / Errors

int azaInit();
void azaDeinit();

void azaDefaultLogFunc(const char* message);

// We use a callback function for all message logging.
// This allows the user to define their own logging output functions
typedef void (*fp_azaLogCallback)(const char* message);

int azaSetLogCallback(fp_azaLogCallback newLogFunc);

extern fp_azaLogCallback azaPrint;

#ifdef __cplusplus
}
#endif

#endif // AZAUDIO_H

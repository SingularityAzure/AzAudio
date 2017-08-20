/*
    File: audio.h
    Author: singularity
    C bindings for core functions

    Most functions will return an int to indicate error status, where 0 is success
*/

//#define SYS_AUDIO_NO_STDIO // to remove stdio dependency

#ifndef SYS_AUDIO_H
#define SYS_AUDIO_H

#include <portaudio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Error handling
#define AZA_SUCCESS 0
#define AZA_ERROR_NULL_POINTER 1

// We use a callback function for all message logging.
// This allows the user to define their own logging output functions
typedef void (*fpLogCallback)(const char* message);

// void newLogFunc(const char* message);
int azaSetLogCallback(fpLogCallback newLogFunc);

void azaHelloWorld();

#ifdef __cplusplus
}
#endif

#endif // SYS_AUDIO_H

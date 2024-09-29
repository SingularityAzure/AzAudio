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

extern const unsigned short azaVersionMajor;
extern const unsigned short azaVersionMinor;
extern const unsigned short azaVersionPatch;

#define AZA_VERSION_FORMAT_STR "%hu.%hu.%hu"
#define AZA_VERSION_ARGS azaVersionMajor, azaVersionMinor, azaVersionPatch

typedef enum AzaLogLevel {
	AZA_LOG_LEVEL_NONE=0,
	AZA_LOG_LEVEL_ERROR,
	AZA_LOG_LEVEL_INFO,
	AZA_LOG_LEVEL_TRACE,
} AzaLogLevel;
extern AzaLogLevel azaLogLevel;

typedef struct azaAllocatorCallbacks {
	// returns zero-initialized memory aligned to at least an 8-byte boundary
	void* (*fp_calloc)(size_t count, size_t size);
	// returns uninitialized memory aligned to at least an 8-byte boundary
	void* (*fp_malloc)(size_t size);
	// frees a block of memory that had been previously returned from fp_calloc
	void (*fp_free)(void *block);
} azaAllocatorCallbacks;
extern azaAllocatorCallbacks azaAllocator;
static inline void* aza_calloc(size_t count, size_t size) {
	return azaAllocator.fp_calloc(count, size);
}
static inline void* aza_malloc(size_t size) {
	return azaAllocator.fp_malloc(size);
}
static inline void aza_free(void *block) {
	azaAllocator.fp_free(block);
}

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

void azaLogDefault(AzaLogLevel level, const char* format, ...);

// We use a callback function for all message logging.
// This allows the user to define their own logging output functions
typedef void (*fp_azaLogCallback)(AzaLogLevel level, const char* format, ...);

void azaSetLogCallback(fp_azaLogCallback newLogFunc);

extern fp_azaLogCallback azaLog;

#ifdef __cplusplus
}
#endif

#endif // AZAUDIO_H

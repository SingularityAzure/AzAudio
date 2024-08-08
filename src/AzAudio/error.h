/*
	File: error.h
	Author: Philip Haynes
	Defining error codes.
*/

#ifndef AZAUDIO_ERROR_H
#define AZAUDIO_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
	// The operation completed successfully
	AZA_SUCCESS=0,
	// A backend is not available on this system
	AZA_ERROR_BACKEND_UNAVAILABLE,
	// Failed to initialize a backend
	AZA_ERROR_BACKEND_LOAD_ERROR,
	// A backend produced an error
	AZA_ERROR_BACKEND_ERROR,
	// A pointer was unexpectedly null
	AZA_ERROR_NULL_POINTER,
	// A dsp function was given a buffer with no channels
	AZA_ERROR_INVALID_CHANNEL_COUNT,
	// A dsp function was given a buffer with no frames
	AZA_ERROR_INVALID_FRAME_COUNT,
	// Something wasn't configured right... check stderr
	AZA_ERROR_INVALID_CONFIGURATION,
};

#ifdef __cplusplus
}
#endif

#endif // AZAUDIO_ERROR_H
/*
    File: audio.h
    Author: singularity
    C bindings for core functions

    Most functions will return an int to indicate error status, where 0 is success
*/

#define SYS_AUDIO_NO_STDIO // to remove stdio dependency

#ifndef SYS_AUDIO_H
#define SYS_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

// Error handling
#define AZA_SUCCESS 0
#define AZA_ERROR_NULL_POINTER 1
#define AZA_ERROR_PORTAUDIO 2

// We use a callback function for all message logging.
// This allows the user to define their own logging output functions
typedef void (*fpLogCallback)(const char* message);

#define AZA_RMS_BUFFER_SIZE 2048

typedef struct {
    float squared;
    float buffer[AZA_RMS_BUFFER_SIZE];
    int index;
} azaRmsData;
azaRmsData azaRmsDataInit();

typedef struct {
    azaRmsData rms;
    float attenuation;
} azaCompressorData;
azaCompressorData azaCompressorDataInit();

// void newLogFunc(const char* message);
int azaSetLogCallback(fpLogCallback newLogFunc);

typedef struct {
    void *stream;
    azaCompressorData *compressorData;
} azaStream;

int azaMicTestStart(azaStream *stream);

int azaMicTestStop(azaStream *stream);

float azaCompressor(float input, azaCompressorData *data, float samplerate,
            float threshold, float ratio, float attack, float decay);

#ifdef __cplusplus
}
#endif

#endif // SYS_AUDIO_H

/*
    File: audio.c
    Author: singularity
*/

#include "audio.h"

#include <portaudio.h>
#include <math.h>
#include <stdlib.h>

#ifndef SYS_AUDIO_NO_STDIO
#include <stdio.h>
#else
#ifndef NULL
#define NULL 0
#endif
#endif

int azaError;

static void azaDefaultLogFunc(const char* message) {
    #ifndef SYS_AUDIO_NO_STDIO
    printf("%s\n",message);
    #endif
}

fpLogCallback azaPrint = azaDefaultLogFunc;

int azaSetLogCallback(fpLogCallback newLogFunc) {
    if (newLogFunc != NULL) {
        azaPrint = newLogFunc;
        azaError = AZA_SUCCESS;
    } else {
        azaError = AZA_ERROR_NULL_POINTER;
    }
    return azaError;
}

float azaSoftLimiter(float input) {
    float output;
    if (input > 1.0f)
        output = 1.0f;
    else if (input < -1.0f)
        output = -1.0f;
    else
        output = input;
    output = 1.5 * output - 0.5f * output * output * output;
    return output;
}

azaRmsData azaRmsDataInit() {
    azaRmsData data;
    data.squared = 0.0f;
    for (int i = 0; i < AZA_RMS_BUFFER_SIZE; i++) {
        data.buffer[i] = 0.0f;
    }
    data.index = 0;
    return data;
}

float azaRms(float input, azaRmsData *data) {
    // Buffer for root mean square detection
    data->squared -= data->buffer[data->index];
    data->buffer[data->index] = input * input;
    data->squared += data->buffer[data->index++];
    if (data->index >= AZA_RMS_BUFFER_SIZE)
        data->index = 0;
    return sqrtf(data->squared/AZA_RMS_BUFFER_SIZE);
}

azaCompressorData azaCompressorDataInit() {
    azaCompressorData data;
    data.rms = azaRmsDataInit();
    data.attenuation = 0.0f;
    return data;
}

float azaCompressor(float input, azaCompressorData *data, float samplerate,
            float threshold, float ratio, float attack, float decay) {
    float rms = azaRms(input, &data->rms);
    float t = samplerate / 1000.0f; // millisecond units
    if (rms > data->attenuation) {
        data->attenuation = 1.0 + expf(-1.0f / (attack * t)) * (data->attenuation - 1.0);
    } else {
        data->attenuation = expf(-1.0f / (decay * t)) * data->attenuation;
    }
    float gain;
    if (data->attenuation > threshold) {
        gain = (1.0f - 1.0f / ratio) * (threshold - data->attenuation);
    } else {
        gain = 0.0f;
    }
    return input * (1.0f + gain);
}

static int azaGNumNoInputs = 0;
/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int azaPortAudioCallback( const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData )
{
    float *out = (float*)outputBuffer;
    const float *in = (const float*)inputBuffer;
    unsigned int i;
    (void) timeInfo; /* Prevent unused variable warnings. */
    (void) statusFlags;
    azaCompressorData *compressorData = (azaCompressorData*)userData;

    if (inputBuffer == NULL)
    {
        for(i=0; i<framesPerBuffer; i++)
        {
            *out++ = 0;  /* left - silent */
            *out++ = 0;  /* right - silent */
        }
        azaGNumNoInputs += 1;
    }
    else
    {
        for (i=0; i<framesPerBuffer; i++)
        {
            *out++ = azaSoftLimiter(azaCompressor(*(in++)*16.0f, &compressorData[0], 44100.0f, 0.1f, 4.0f, 50.0f, 200.0f));
            *out++ = azaSoftLimiter(azaCompressor(*(in++)*16.0f, &compressorData[1], 44100.0f, 0.1f, 4.0f, 50.0f, 200.0f));
        }
    }

    return paContinue;
}

int azaMicTestStart(azaStream *stream) {
    PaStreamParameters inputParameters, outputParameters;
    PaError err;

    err = Pa_Initialize();
    if (err != paNoError) {
        azaPrint("Error: Failed to initialize PortAudio");
        azaError = AZA_ERROR_PORTAUDIO;
        return azaError;
    }

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice) {
        azaPrint("Error: No default input device");
        azaError = AZA_ERROR_PORTAUDIO;
        return azaError;
    }
    inputParameters.channelCount = 2;       /* stereo input */
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice) {
        azaPrint("Error: No default output device");
        azaError = AZA_ERROR_PORTAUDIO;
        return azaError;
    }
    outputParameters.channelCount = 2;       /* stereo output */
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    stream->compressorData = (azaCompressorData*)malloc(sizeof(azaCompressorData) * 2);
    stream->compressorData[0] = azaCompressorDataInit();
    stream->compressorData[1] = azaCompressorDataInit();

    err = Pa_OpenStream(
            (PaStream**)&stream->stream,
            &inputParameters,
            &outputParameters,
            44100,
            64,
            0, /* paClipOff, */  /* we won't output out of range samples so don't bother clipping them */
            azaPortAudioCallback,
            (void*)stream->compressorData);
    if (err != paNoError) {
        azaPrint("Error: Failed to open PortAudio stream");
        azaError = AZA_ERROR_PORTAUDIO;
        return azaError;
    }

    err = Pa_StartStream((PaStream*)stream->stream);
    if (err != paNoError) {
        azaPrint("Error: Failed to start PortAudio stream");
        azaError = AZA_ERROR_PORTAUDIO;
        return azaError;
    }
    azaError = AZA_SUCCESS;
    return azaError;
}

int azaMicTestStop(azaStream *stream) {
    PaError err;
    if (stream == NULL) {
        azaPrint("Error: stream is null");
        azaError = AZA_ERROR_NULL_POINTER;
        return azaError;
    }
    err = Pa_CloseStream((PaStream*)stream->stream);
    if (err != paNoError) {
        azaPrint("Error: Failed to close PortAudio stream");
        azaError = AZA_ERROR_PORTAUDIO;
        return azaError;
    }
    free(stream->compressorData);

    Pa_Terminate();
    azaError = AZA_SUCCESS;
    return azaError;
}

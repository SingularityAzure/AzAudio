/*
    File: audio.c
    Author: singularity
*/

#include "audio.h"

#include <portaudio.h>
#include <math.h>
#include <stdlib.h>

#ifndef AZURE_AUDIO_NO_STDIO
#include <stdio.h>
#else
#ifndef NULL
#define NULL 0
#endif
#endif

int azaError;

fpLogCallback azaPrint;

int azaInit() {
    azaPrint = azaDefaultLogFunc;
    azaError = AZA_SUCCESS;
    return azaError;
}

int azaGetError() {
    return azaError;
}

void azaDefaultLogFunc(const char* message) {
    #ifndef AZURE_AUDIO_NO_STDIO
    printf("AzureAudio: %s\n",message);
    #endif
}

int azaSetLogCallback(fpLogCallback newLogFunc) {
    if (newLogFunc != NULL) {
        azaPrint = newLogFunc;
        azaError = AZA_SUCCESS;
    } else {
        azaError = AZA_ERROR_NULL_POINTER;
    }
    return azaError;
}

void azaRmsDataInit(azaRmsData *data) {
    data->squared = 0.0f;
    for (int i = 0; i < AZURE_AUDIO_RMS_SAMPLES; i++) {
        data->buffer[i] = 0.0f;
    }
    data->index = 0;
}

void azaLookaheadLimiterDataInit(azaLookaheadLimiterData *data) {
    for (int i = 0; i < AZURE_AUDIO_LOOKAHEAD_SAMPLES; i++) {
        data->gainBuffer[i] = 0.0f;
        data->valBuffer[i] = 0.0f;
    }
}

void azaLowPassDataInit(azaLowPassData *data) {
    data->output = 0.0f;
}

void azaHighPassDataInit(azaHighPassData *data) {
    data->output = 0.0f;
}

void azaCompressorDataInit(azaCompressorData *data) {
    azaRmsDataInit(&data->rms);
    data->attenuation = 0.0f;
}

void azaDelayDataInit(azaDelayData *data, int samples) {
    data->buffer = (float*)malloc(sizeof(float) * samples);
    for (int i = 0; i < samples; i++) {
        data->buffer[i] = 0.0f;
    }
    data->index = 0;
    data->samples = samples;
}

void azaDelayDataClean(azaDelayData *data) {
    free(data->buffer);
}

void azaReverbDataInit(azaReverbData *data, int samples[AZURE_AUDIO_REVERB_DELAY_COUNT]) {
    for (int i = 0; i < AZURE_AUDIO_REVERB_DELAY_COUNT; i++) {
        azaDelayDataInit(&data->delay[i], samples[i]);
        azaLowPassDataInit(&data->lowPass[i]);
    }
}

void azaReverbDataClean(azaReverbData *data) {
    for (int i = 0; i < AZURE_AUDIO_REVERB_DELAY_COUNT; i++) {
        azaDelayDataClean(&data->delay[i]);
    }
}

void azaMixDataInit(azaMixData *data) {
    for (int i = 0; i < 2; i++) {
        data->highPassData[i].samplerate = 44100.0f;
        data->highPassData[i].frequency = 40.0f;
        data->delayData[i].amount = 1.0f;
        data->delayData[i].feedback = 0.85f;
        data->reverbData[i].amount = 0.2f;
        data->reverbData[i].roomsize = 3.0f;
        data->reverbData[i].color = 0.3f;
        data->compressorData[i].samplerate = 44100.0f;
        data->compressorData[i].threshold = -36.0f;
        data->compressorData[i].ratio = 4.0f;
        data->compressorData[i].attack = 50.0f;
        data->compressorData[i].decay = 200.0f;
        data->limiterData[i].gain = 24.0f;
    }
    azaLookaheadLimiterDataInit(&data->limiterData[0]);
    azaLookaheadLimiterDataInit(&data->limiterData[1]);
    azaCompressorDataInit(&data->compressorData[0]);
    azaCompressorDataInit(&data->compressorData[1]);
    azaDelayDataInit(&data->delayData[0], 1000);
    azaDelayDataInit(&data->delayData[1], 1000);
    //int samples[AZURE_AUDIO_REVERB_DELAY_COUNT] = {225, 556, 441, 341};
    int samples[AZURE_AUDIO_REVERB_DELAY_COUNT] = {1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116, 2111, 2133, 225, 556, 441, 341, 713};
    int samplesL[AZURE_AUDIO_REVERB_DELAY_COUNT];
    int samplesR[AZURE_AUDIO_REVERB_DELAY_COUNT];
    for (int i = 0; i < AZURE_AUDIO_REVERB_DELAY_COUNT; i++) {
        samplesL[i] = samples[i];
        samplesR[i] = samples[i] + 23;
    }
    azaReverbDataInit(&data->reverbData[0], samplesL);
    azaReverbDataInit(&data->reverbData[1], samplesR);
    azaHighPassDataInit(&data->highPassData[0]);
    azaHighPassDataInit(&data->highPassData[1]);
}

void azaMixDataClean(azaMixData *data) {
    azaDelayDataClean(&data->delayData[0]);
    azaDelayDataClean(&data->delayData[1]);
    azaReverbDataClean(&data->reverbData[0]);
    azaReverbDataClean(&data->reverbData[1]);
}

int azaCubicLimiter(float *input, float *output, int frames, int channels) {
    if (input == NULL || output == NULL) {
        azaError = AZA_ERROR_NULL_POINTER;
        return azaError;
    }
    if (channels < 1) {
        azaError = AZA_ERROR_INVALID_CHANNEL_COUNT;
        return azaError;
    }
    if (frames < 1) {
        azaError = AZA_ERROR_INVALID_FRAME_COUNT;
        return azaError;
    }
    for (int i = 0; i < frames*channels; i++) {
        if (input[i] > 1.0f)
            output[i] = 1.0f;
        else if (input[i] < -1.0f)
            output[i] = -1.0f;
        else
            output[i] = input[i];
        output[i] = 1.5 * output[i] - 0.5f * output[i] * output[i] * output[i];
    }
    azaError = AZA_SUCCESS;
    return azaError;
}

int azaRms(float *input, float *output, azaRmsData *data, int frames, int channels) {
    if (input == NULL || output == NULL || data == NULL) {
        azaError = AZA_ERROR_NULL_POINTER;
        return azaError;
    }
    if (channels < 1) {
        azaError = AZA_ERROR_INVALID_CHANNEL_COUNT;
        return azaError;
    }
    if (frames < 1) {
        azaError = AZA_ERROR_INVALID_FRAME_COUNT;
        return azaError;
    }
    for (int i = 0; i < frames*channels; i++) {
        azaRmsData *datum = &data[i % channels];

        datum->squared -= datum->buffer[datum->index];
        datum->buffer[datum->index] = input[i] * input[i];
        datum->squared += datum->buffer[datum->index++];
        if (datum->index >= AZURE_AUDIO_RMS_SAMPLES)
            datum->index = 0;

        output[i] = sqrtf(datum->squared/AZURE_AUDIO_RMS_SAMPLES);
    }
    azaError = AZA_SUCCESS;
    return azaError;
}

int azaLookaheadLimiter(float *input, float *output, azaLookaheadLimiterData *data, int frames, int channels) {
    if (input == NULL || output == NULL || data == NULL) {
        azaError = AZA_ERROR_NULL_POINTER;
        return azaError;
    }
    if (channels < 1) {
        azaError = AZA_ERROR_INVALID_CHANNEL_COUNT;
        return azaError;
    }
    if (frames < 1) {
        azaError = AZA_ERROR_INVALID_FRAME_COUNT;
        return azaError;
    }
    for (int i = 0; i < frames*channels; i++) {
        azaLookaheadLimiterData *datum = &data[i % channels];

        float peak = input[i];
        float gain = datum->gain;
        if (peak < 0.0f)
            peak = -peak;
        peak = log2f(peak)*6.0f + gain;
        if (peak < 0.0f)
            peak = 0.0f;
        datum->sum += peak - datum->gainBuffer[datum->index];
        float average = datum->sum / AZURE_AUDIO_LOOKAHEAD_SAMPLES;
        if (average > peak) {
            datum->sum += average - peak;
            peak = average;
        }
        datum->gainBuffer[datum->index] = peak;

        datum->valBuffer[datum->index] = input[i];

        datum->index = (datum->index+1)%AZURE_AUDIO_LOOKAHEAD_SAMPLES;

        if (average > datum->gainBuffer[datum->index])
            gain -= average;
        else
            gain -= datum->gainBuffer[datum->index];
        float out = datum->valBuffer[datum->index] * powf(2.0f,gain/6.0f);
        if (out < -1.0f)
            out = -1.0f;
        else if (out > 1.0f)
            out = 1.0f;
        output[i] = out;
    }
    azaError = AZA_SUCCESS;
    return azaError;
}

int azaLowPass(float *input, float *output, azaLowPassData *data, int frames, int channels) {
    if (input == NULL || output == NULL || data == NULL) {
        azaError = AZA_ERROR_NULL_POINTER;
        return azaError;
    }
    if (channels < 1) {
        azaError = AZA_ERROR_INVALID_CHANNEL_COUNT;
        return azaError;
    }
    if (frames < 1) {
        azaError = AZA_ERROR_INVALID_FRAME_COUNT;
        return azaError;
    }

    for (int i = 0; i < frames*channels; i++) {
        azaLowPassData *datum = &data[i % channels];

        float amount = expf(-1.0f * (datum->frequency / datum->samplerate));
        datum->output = input[i] + amount * (datum->output - input[i]);
        output[i] = datum->output;
    }
    azaError = AZA_SUCCESS;
    return azaError;
}

int azaHighPass(float *input, float *output, azaHighPassData *data, int frames, int channels) {
    if (input == NULL || output == NULL || data == NULL) {
        azaError = AZA_ERROR_NULL_POINTER;
        return azaError;
    }
    if (channels < 1) {
        azaError = AZA_ERROR_INVALID_CHANNEL_COUNT;
        return azaError;
    }
    if (frames < 1) {
        azaError = AZA_ERROR_INVALID_FRAME_COUNT;
        return azaError;
    }

    for (int i = 0; i < frames*channels; i++) {
        azaHighPassData *datum = &data[i % channels];

        float amount = expf(-1.0f * (datum->frequency / datum->samplerate));
        datum->output = input[i] + amount * (datum->output - input[i]);
        output[i] = input[i] - datum->output;
    }
    azaError = AZA_SUCCESS;
    return azaError;
}

int azaCompressor(float *input, float *output, azaCompressorData *data, int frames, int channels) {
    if (input == NULL || output == NULL || data == NULL) {
        azaError = AZA_ERROR_NULL_POINTER;
        return azaError;
    }
    if (channels < 1) {
        azaError = AZA_ERROR_INVALID_CHANNEL_COUNT;
        return azaError;
    }
    if (frames < 1) {
        azaError = AZA_ERROR_INVALID_FRAME_COUNT;
        return azaError;
    }

    for (int i = 0; i < frames*channels; i++) {
        azaCompressorData *datum = &data[i % channels];

        float rms;
        azaRms(&input[i], &rms, &datum->rms, 1, 1);
        rms = log2f(rms)*6.0f;
        float t = datum->samplerate / 1000.0f; // millisecond units
        float mult;
        if (datum->ratio > 0.0f) {
            mult = (1.0f - 1.0f / datum->ratio);
        } else {
            mult = -datum->ratio;
        }
        if (rms > datum->attenuation) {
            datum->attenuation = rms + expf(-1.0f / (datum->attack * t)) * (datum->attenuation - rms);
        } else {
            datum->attenuation = rms + expf(-1.0f / (datum->decay * t)) * (datum->attenuation - rms);
        }
        float gain;
        if (datum->attenuation > datum->threshold) {
            gain = mult * (datum->threshold - datum->attenuation);
        } else {
            gain = 0.0f;
        }
        datum->gain = gain;
        output[i] = input[i] * powf(2.0f,gain/6.0f);
    }
    azaError = AZA_SUCCESS;
    return azaError;
}

int azaDelay(float *input, float *output, azaDelayData *data, int frames, int channels) {
    if (input == NULL || output == NULL || data == NULL) {
        azaError = AZA_ERROR_NULL_POINTER;
        return azaError;
    }
    if (channels < 1) {
        azaError = AZA_ERROR_INVALID_CHANNEL_COUNT;
        return azaError;
    }
    if (frames < 1) {
        azaError = AZA_ERROR_INVALID_FRAME_COUNT;
        return azaError;
    }

    for (int i = 0; i < frames*channels; i++) {
        azaDelayData *datum = &data[i % channels];

        datum->buffer[datum->index] = input[i] + datum->buffer[datum->index] * datum->feedback;
        datum->index++;
        if (datum->index >= datum->samples) {
            datum->index = 0;
        }
        output[i] = datum->buffer[datum->index] * datum->amount + input[i];
    }
    azaError = AZA_SUCCESS;
    return azaError;
}

int azaReverb(float *input, float *output, azaReverbData *data, int frames, int channels) {
    if (input == NULL || output == NULL || data == NULL) {
        azaError = AZA_ERROR_NULL_POINTER;
        return azaError;
    }
    if (channels < 1) {
        azaError = AZA_ERROR_INVALID_CHANNEL_COUNT;
        return azaError;
    }
    if (frames < 1) {
        azaError = AZA_ERROR_INVALID_FRAME_COUNT;
        return azaError;
    }

    for (int i = 0; i < frames*channels; i++) {
        azaReverbData *datum = &data[i % channels];

        float out = input[i];
        float feedback = 0.98f - (0.2f / datum->roomsize);
        float color = datum->color * 4000.0f;
        for (int ii = 0; ii < AZURE_AUDIO_REVERB_DELAY_COUNT*2/3; ii++) {
            datum->delay[ii].feedback = feedback;
            datum->delay[ii].amount = 1.0f;
            datum->lowPass[ii].samplerate = 44100.0f;
            datum->lowPass[ii].frequency = color;
            float early = input[i];
            azaLowPass(&early, &early, &datum->lowPass[ii], 1, 1);
            azaDelay(&early, &early, &datum->delay[ii], 1, 1);
            out += early - input[i];
        }
        for (int ii = AZURE_AUDIO_REVERB_DELAY_COUNT*2/3; ii < AZURE_AUDIO_REVERB_DELAY_COUNT; ii++) {
            datum->delay[ii].feedback = (float)(ii+8) / (AZURE_AUDIO_REVERB_DELAY_COUNT + 8.0f);
            datum->delay[ii].amount = 1.0f;
            datum->lowPass[ii].samplerate = 44100.0f;
            datum->lowPass[ii].frequency = color*2.0f;
            float diffuse = out/(float)(1+ii);
            azaLowPass(&diffuse, &diffuse, &datum->lowPass[ii], 1, 1);
            azaDelay(&diffuse, &diffuse, &datum->delay[ii], 1, 1);
            out += diffuse - out/(float)(1+ii);
        }
        out *= datum->amount;
        output[i] = out + input[i];
    }
    azaError = AZA_SUCCESS;
    return azaError;
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
    azaMixData *mixData = (azaMixData*)userData;

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
        azaError = azaDelay((float*)in, out, mixData->delayData, framesPerBuffer, 2);
        if (azaError) {
            return azaError;
        }
        azaError = azaReverb(out, out, mixData->reverbData, framesPerBuffer, 2);
        if (azaError) {
            return azaError;
        }
        azaError = azaHighPass(out, out, mixData->highPassData, framesPerBuffer, 2);
        if (azaError) {
            return azaError;
        }
        azaError = azaCompressor(out, out, mixData->compressorData, framesPerBuffer, 2);
        if (azaError) {
            return azaError;
        }
        azaError = azaLookaheadLimiter(out, out, mixData->limiterData, framesPerBuffer, 2);
        if (azaError) {
            return azaError;
        }
    }
    return paContinue;
}

int azaMicTestStart(azaStream *stream, azaMixData *data) {
    PaStreamParameters inputParameters, outputParameters;
    PaError err;

    azaPrint("Starting mic test");

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
    inputParameters.channelCount = 2;       /* mono input */
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = 512.0 / 44100.0;
    //inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice) {
        azaPrint("Error: No default output device");
        azaError = AZA_ERROR_PORTAUDIO;
        return azaError;
    }
    outputParameters.channelCount = 2;       /* stereo output */
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = 512.0 / 44100.0;
    //outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
            (PaStream**)&stream->stream,
            &inputParameters,
            &outputParameters,
            44100,
            64,
            0, /* paClipOff, */  /* we won't output out of range samples so don't bother clipping them */
            azaPortAudioCallback,
            (void*)data);
    if (err != paNoError) {
        azaPrint("Error: Failed to open PortAudio stream");
        azaError = AZA_ERROR_PORTAUDIO;
        return azaError;
    }
    const PaStreamInfo *streamInfo = Pa_GetStreamInfo((PaStream*)stream->stream);
    double samplerate = streamInfo->sampleRate;
    printf("Stream latency input: %f output: %f samplerate: %f\n",samplerate*streamInfo->inputLatency, samplerate*streamInfo->outputLatency, samplerate);

    err = Pa_StartStream((PaStream*)stream->stream);
    if (err != paNoError) {
        azaPrint("Error: Failed to start PortAudio stream");
        azaError = AZA_ERROR_PORTAUDIO;
        return azaError;
    }
    azaError = AZA_SUCCESS;
    return azaError;
}

int azaMicTestStop(azaStream *stream, azaMixData *data) {
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

    Pa_Terminate();
    azaError = AZA_SUCCESS;
    return azaError;
}

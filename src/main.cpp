/*
	File: main.cpp
	Author: singularity
	Simple test program for our library
*/

#include <iostream>
#include <vector>

#include "log.hpp"
#include "audio.h"

#ifdef __unix
#include <csignal>
#include <cstdlib>
#include <cmath>
#include <execinfo.h>
#include <unistd.h>

void handler(int sig) {
	void *array[50];
	size_t size = backtrace(array, 50);
	char **strings;
	strings = backtrace_symbols(array, size);
	sys::cout <<  "Error: signal " << sig << std::endl;
	for (uint32_t i = 0; i < size; i++) {
		sys::cout << strings[i] << std::endl;
	}
	free(strings);
	exit(1);
}
#endif

void logCallback(const char* message) {
	sys::cout << "AzAudio: " << message << std::endl;
}

azaLookaheadLimiterData limiterData[AZA_CHANNELS];
azaCompressorData compressorData[AZA_CHANNELS];
azaDelayData delayData[AZA_CHANNELS];
azaDelayData delay2Data[AZA_CHANNELS];
azaDelayData delay3Data[AZA_CHANNELS];
azaReverbData reverbData[AZA_CHANNELS];
azaHighPassData highPassData[AZA_CHANNELS];
azaGateData gateData[AZA_CHANNELS];

std::vector<float> buffer;
size_t lastBufferSize=0;

int mixCallbackOutput(const float *input, float *output, unsigned long frames, int channels, void *userData) {
	if (buffer.size() == lastBufferSize && buffer.size() > (frames*channels)*2) {
		buffer.erase(buffer.end() - (frames*channels), buffer.end());
	}
	lastBufferSize = buffer.size();
	// printf("buffer size: %d\n", buffer.size() / channels);
	// printf("output has ");
	size_t i = 0;
	for (; i < std::min(frames*channels, buffer.size()); i++) {
		output[i] = buffer[i];
	}
	buffer.erase(buffer.begin(), buffer.begin() + i);
	for (; i < frames*channels; i++) {
		output[i] = 0.0f;
	}
	if (azaGate(output, output, gateData, frames, channels)) {
		return azaGetError();
	}
	// printf("gate gain: %f\n", gateData->gain);
	if (azaDelay(output, output, delayData, frames, channels)) {
		return azaGetError();
	}
	if (azaDelay(output, output, delay2Data, frames, channels)) {
		return azaGetError();
	}
	if (azaDelay(output, output, delay3Data, frames, channels)) {
		return azaGetError();
	}
	if (azaReverb(output, output, reverbData, frames, channels)) {
		return azaGetError();
	}
	if (azaHighPass(output, output, highPassData, frames, channels)) {
		return azaGetError();
	}
	if (azaCompressor(output, output, compressorData, frames, channels)) {
		return azaGetError();
	}
	if (azaLookaheadLimiter(output, output, limiterData, frames, channels)) {
		return azaGetError();
	}
	return AZA_SUCCESS;
}

int mixCallbackInput(const float *input, float *output, unsigned long frames, int channels, void *userData) {
	size_t b_i = buffer.size();
	buffer.resize(buffer.size() + frames*channels);
	for (unsigned long i = 0; i < frames*channels; i++) {
		buffer[b_i + i] = input[i];
	}
	// printf("input has  ");
	return AZA_SUCCESS;
}

int main(int argumentCount, char** argumentValues) {
	#ifdef __unix
	signal(SIGSEGV, handler);
	#endif
	try {
		azaInit();
		for (int c = 0; c < AZA_CHANNELS; c++) {
			gateData[c].threshold = -21.0f;
			gateData[c].attack = 1.0f;
			gateData[c].decay = 200.0f;
			azaGateDataInit(&gateData[c]);
			
			delayData[c].feedback = 0.8f;
			delayData[c].amount = 0.2f;
			delayData[c].samples = AZA_SAMPLERATE * 2;
			azaDelayDataInit(&delayData[c]);
			 
			delay2Data[c].feedback = 0.8f;
			delay2Data[c].amount = 0.2f;
			delay2Data[c].samples = AZA_SAMPLERATE * 3;
			azaDelayDataInit(&delay2Data[c]);
			
			delay3Data[c].feedback = 0.8f;
			delay3Data[c].amount = 0.2f;
			delay3Data[c].samples = AZA_SAMPLERATE * 5;
			azaDelayDataInit(&delay3Data[c]);
			
			reverbData[c].amount = 1.0f;
			reverbData[c].roomsize = 100.0f;
			reverbData[c].color = 1.0f;
			reverbData[c].samplesOffset = c * 377;
			azaReverbDataInit(&reverbData[c]);
			
			highPassData[c].samplerate = AZA_SAMPLERATE;
			highPassData[c].frequency = 80.0f;
			azaHighPassDataInit(&highPassData[c]);
			
			compressorData[c].samplerate = AZA_SAMPLERATE;
			compressorData[c].threshold = -24.0f;
			compressorData[c].ratio = 10.0f;
			compressorData[c].attack = 100.0f;
			compressorData[c].decay = 200.0f;
			azaCompressorDataInit(&compressorData[c]);
			
			limiterData[c].gain = 0.0f;
			azaLookaheadLimiterDataInit(&limiterData[c]);
		}
		azaSetLogCallback(logCallback);
		azaStream streamInput, streamOutput;
		if (azaInitStream(&streamInput, "default", true, mixCallbackInput) != AZA_SUCCESS) {
			throw std::runtime_error("Failed to init input stream!");
		}
		if (azaInitStream(&streamOutput, "default", false, mixCallbackOutput) != AZA_SUCCESS) {
			throw std::runtime_error("Failed to init output stream!");
		}
		std::cout << "Press ENTER to stop" << std::endl;
		std::cin.get();
		azaDeinitStream(&streamInput);
		azaDeinitStream(&streamOutput);
		for (int c = 0; c < AZA_CHANNELS; c++) {
			azaDelayDataClean(&delayData[c]);
			azaReverbDataClean(&reverbData[c]);
		}
		
		azaClean();
	} catch (std::runtime_error& e) {
		sys::cout << "Runtime Error: " << e.what() << std::endl;
	}
	return 0;
}

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
azaReverbData reverbData[AZA_CHANNELS];

std::vector<float> buffer;

int mixCallbackOutput(const float *input, float *output, unsigned long frames, int channels, void *userData) {
	size_t i = 0;
	for (; i < std::min(frames*channels, buffer.size()); i++) {
		output[i] = buffer[i];
	}
	for (; i < frames*channels; i++) {
		output[i] = 0.0f;
	}
	azaDelay(output, output, delayData, frames, channels);
	azaReverb(output, output, reverbData, frames, channels);
	azaCompressor(output, output, compressorData, frames, channels);
	azaLookaheadLimiter(output, output, limiterData, frames, channels);
	return AZA_SUCCESS;
}

int mixCallbackInput(const float *input, float *output, unsigned long frames, int channels, void *userData) {
	buffer.resize(frames*channels);
	for (unsigned long i = 0; i < frames*channels; i++) {
		buffer[i] = input[i];
	}
	return AZA_SUCCESS;
}

int main(int argumentCount, char** argumentValues) {
	#ifdef __unix
	signal(SIGSEGV, handler);
	#endif
	try {
		azaInit();
		for (int c = 0; c < AZA_CHANNELS; c++) {
			delayData[c].feedback = 0.5f;
			delayData[c].amount = 0.5f;
			delayData[c].samples = AZA_SAMPLERATE / 3;
			azaDelayDataInit(&delayData[c]);
			
			reverbData[c].amount = 0.2f;
			reverbData[c].roomsize = 100.0f;
			reverbData[c].color = 1.0f;
			reverbData[c].samplesOffset = c * 23;
			azaReverbDataInit(&reverbData[c]);
			
			compressorData[c].samplerate = AZA_SAMPLERATE;
			compressorData[c].threshold = -12.0f;
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
		sys::cout << "Press ENTER to stop" << std::endl;
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

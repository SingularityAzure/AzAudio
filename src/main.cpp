/*
	File: main.cpp
	Author: singularity
	Simple test program for our library
*/

#include <iostream>
#include <vector>

#include "log.hpp"
#include "AzAudio/AzAudio.h"
#include "AzAudio/error.h"

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

azaLookaheadLimiterData limiterData[AZA_CHANNELS_DEFAULT];
azaCompressorData compressorData[AZA_CHANNELS_DEFAULT];
azaDelayData delayData[AZA_CHANNELS_DEFAULT];
azaDelayData delay2Data[AZA_CHANNELS_DEFAULT];
azaDelayData delay3Data[AZA_CHANNELS_DEFAULT];
azaReverbData reverbData[AZA_CHANNELS_DEFAULT];
azaFilterData highPassData[AZA_CHANNELS_DEFAULT];
azaGateData gateData[AZA_CHANNELS_DEFAULT];

std::vector<float> micBuffer;
size_t lastOutputBufferSize=0;
size_t lastInputBufferSize=0;
size_t numOutputBuffers=0;
size_t numInputBuffers=0;

int mixCallbackOutput(azaBuffer buffer, void *userData) {
	size_t count = buffer.frames*buffer.channels;
	numOutputBuffers++;
	if (micBuffer.size() == lastOutputBufferSize && micBuffer.size() > count*2) {
		sys::cout << "Shrunk!" << std::endl;
		// Crossfade from new end to actual end
		float t = 0.0f;
		for (size_t i = micBuffer.size()-255*buffer.channels; i < micBuffer.size(); i++) {
			if (i % buffer.channels == 0) {
				t = std::min(1.0f, t + 1.0f / 256.0f);
			}
			float *dst = &micBuffer[i - count];
			float src = micBuffer[i];
			*dst = *dst + (src - *dst) * t;
		}
		micBuffer.erase(micBuffer.end() - count, micBuffer.end());
	}
	lastOutputBufferSize = micBuffer.size();
	// printf("micBuffer size: %d\n", micBuffer.size() / buffer.channels);
	// printf("output has ");
	size_t i = 0;
	static float lastSample = 0.0f;
	static float fadein = 0.0f;
	for (; i < std::min(count, micBuffer.size()); i++) {
		if (i % buffer.channels == 0) {
			fadein = std::min(1.0f, fadein + 1.0f / 256.0f);
			lastSample = std::max(0.0f, lastSample - 1.0f / 256.0f);
		}
		buffer.samples[i] = lastSample + micBuffer[i] * fadein;
	}
	if (count > micBuffer.size()) {
		fadein = 0.0f;
		if (micBuffer.size()) lastSample = micBuffer.back();
		sys::cout << "Buffer underrun (" << micBuffer.size() << "/" << count << " samples available, last input buffer was " << lastInputBufferSize << " samples and last output buffer was " << count << " samples, had " << numInputBuffers << " input buffers and " << numOutputBuffers << " output buffers so far)" << std::endl;
	}
	micBuffer.erase(micBuffer.begin(), micBuffer.begin() + i);
	for (; i < count; i++) {
		if (i % buffer.channels == 0) {
			lastSample = std::max(0.0f, lastSample - 1.0f / 256.0f);
		}
		buffer.samples[i] = lastSample;
	}
	int err;
	if ((err = azaGate(buffer, gateData))) {
		return err;
	}
	// printf("gate gain: %f\n", gateData->gain);
	if ((err = azaDelay(buffer, delayData))) {
		return err;
	}
	if ((err = azaDelay(buffer, delay2Data))) {
		return err;
	}
	if ((err = azaDelay(buffer, delay3Data))) {
		return err;
	}
	if ((err = azaReverb(buffer, reverbData))) {
		return err;
	}
	if ((err = azaFilter(buffer, highPassData))) {
		return err;
	}
	if ((err = azaCompressor(buffer, compressorData))) {
		return err;
	}
	if ((err = azaLookaheadLimiter(buffer, limiterData))) {
		return err;
	}
	return AZA_SUCCESS;
}

int mixCallbackInput(azaBuffer buffer, void *userData) {
	numInputBuffers++;
	lastInputBufferSize = buffer.frames*buffer.channels;
	size_t b_i = micBuffer.size();
	micBuffer.resize(micBuffer.size() + buffer.frames*buffer.channels);
	for (unsigned long i = 0; i < buffer.frames*buffer.channels; i++) {
		micBuffer[b_i + i] = buffer.samples[i];
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
		{ // Query devices
			size_t numOutputDevices = azaGetDeviceCount(AZA_OUTPUT);
			sys::cout << "Output Devices: " << numOutputDevices << std::endl;
			for (size_t i = 0; i < numOutputDevices; i++) {
				size_t channels = azaGetDeviceChannels(AZA_OUTPUT, i);
				sys::cout << "\t" << azaGetDeviceName(AZA_OUTPUT, i) << " with " << channels << " channels." << std::endl;
			}
			size_t numInputDevices = azaGetDeviceCount(AZA_INPUT);
			sys::cout << "Input Devices: " << numInputDevices << std::endl;
			for (size_t i = 0; i < numInputDevices; i++) {
				size_t channels = azaGetDeviceChannels(AZA_INPUT, i);
				sys::cout << "\t" << azaGetDeviceName(AZA_INPUT, i) << " with " << channels << " channels." << std::endl;
			}
		}
		for (int c = 0; c < AZA_CHANNELS_DEFAULT; c++) {
			gateData[c].threshold = -24.0f;
			gateData[c].attack = 1.0f;
			gateData[c].decay = 200.0f;
			azaGateDataInit(&gateData[c]);
			
			delayData[c].gain = -12.0f;
			delayData[c].gainDry = 0.0f;
			delayData[c].delay = 1234.5f;
			delayData[c].feedback = 0.5f;
			azaDelayDataInit(&delayData[c]);
			 
			delay2Data[c].gain = -12.0f;
			delay2Data[c].gainDry = 0.0f;
			delay2Data[c].delay = 2345.6f;
			delay2Data[c].feedback = 0.5f;
			azaDelayDataInit(&delay2Data[c]);
			
			delay3Data[c].gain = -12.0f;
			delay3Data[c].gainDry = 0.0f;
			delay3Data[c].delay = 3456.7f;
			delay3Data[c].feedback = 0.5f;
			azaDelayDataInit(&delay3Data[c]);
			
			reverbData[c].gain = 0.0f;
			reverbData[c].gainDry = 0.0f;
			reverbData[c].roomsize = 100.0f;
			reverbData[c].color = 1.0f;
			reverbData[c].delay = c * 377.0f / 48000.0f;
			azaReverbDataInit(&reverbData[c]);
			
			highPassData[c].kind = AZA_FILTER_HIGH_PASS;
			highPassData[c].frequency = 100.0f;
			azaFilterDataInit(&highPassData[c]);
			
			compressorData[c].threshold = -24.0f;
			compressorData[c].ratio = 10.0f;
			compressorData[c].attack = 100.0f;
			compressorData[c].decay = 200.0f;
			azaCompressorDataInit(&compressorData[c]);
			
			limiterData[c].gainInput = 12.0f;
			limiterData[c].gainOutput = 0.0f;
			azaLookaheadLimiterDataInit(&limiterData[c]);
		}
		azaSetLogCallback(logCallback);
		azaStream streamInput = {0};
		streamInput.mixCallback = mixCallbackInput;
		streamInput.deviceInterface = AZA_INPUT;
		if (azaStreamInit(&streamInput, "default") != AZA_SUCCESS) {
			throw std::runtime_error("Failed to init input stream!");
		}
		azaStream streamOutput = {0};
		streamOutput.mixCallback = mixCallbackOutput;
		if (azaStreamInit(&streamOutput, "default") != AZA_SUCCESS) {
			throw std::runtime_error("Failed to init output stream!");
		}
		std::cout << "Press ENTER to stop" << std::endl;
		std::cin.get();
		azaStreamDeinit(&streamInput);
		azaStreamDeinit(&streamOutput);
		for (int c = 0; c < AZA_CHANNELS_DEFAULT; c++) {
			azaDelayDataDeinit(&delayData[c]);
			azaReverbDataDeinit(&reverbData[c]);
		}
		
		azaDeinit();
	} catch (std::runtime_error& e) {
		sys::cout << "Runtime Error: " << e.what() << std::endl;
	}
	return 0;
}

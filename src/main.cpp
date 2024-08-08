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
azaHighPassData highPassData[AZA_CHANNELS_DEFAULT];
azaGateData gateData[AZA_CHANNELS_DEFAULT];

std::vector<float> micBuffer;
size_t lastBufferSize=0;

int mixCallbackOutput(azaBuffer buffer, void *userData) {
	if (micBuffer.size() == lastBufferSize && micBuffer.size() > (buffer.frames*buffer.channels)*2) {
		micBuffer.erase(micBuffer.end() - (buffer.frames*buffer.channels), micBuffer.end());
	}
	lastBufferSize = micBuffer.size();
	// printf("micBuffer size: %d\n", micBuffer.size() / buffer.channels);
	// printf("output has ");
	size_t i = 0;
	for (; i < std::min(buffer.frames*buffer.channels, micBuffer.size()); i++) {
		buffer.samples[i] = micBuffer[i];
	}
	micBuffer.erase(micBuffer.begin(), micBuffer.begin() + i);
	for (; i < buffer.frames*buffer.channels; i++) {
		buffer.samples[i] = 0.0f;
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
	if ((err = azaHighPass(buffer, highPassData))) {
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
			
			highPassData[c].frequency = 80.0f;
			azaHighPassDataInit(&highPassData[c]);
			
			compressorData[c].threshold = -24.0f;
			compressorData[c].ratio = 10.0f;
			compressorData[c].attack = 100.0f;
			compressorData[c].decay = 200.0f;
			azaCompressorDataInit(&compressorData[c]);
			
			limiterData[c].gainInput = 0.0f;
			limiterData[c].gainOutput = 0.0f;
			azaLookaheadLimiterDataInit(&limiterData[c]);
		}
		azaSetLogCallback(logCallback);
		azaStream streamInput = {0};
		streamInput.mixCallback = mixCallbackInput;
		streamInput.deviceInterface = AZA_INPUT;
		azaStream streamOutput = {0};
		streamOutput.mixCallback = mixCallbackOutput;
		if (azaStreamInit(&streamInput, "default") != AZA_SUCCESS) {
			throw std::runtime_error("Failed to init input stream!");
		}
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

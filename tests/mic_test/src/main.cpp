/*
	File: main.cpp
	Author: singularity
	Simple test program for our library
*/

#include <iostream>
#include <vector>

#include <cstdarg>

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

void logCallback(AzaLogLevel level, const char* format, ...) {
	if (level > azaLogLevel) return;
	char buffer[1024];
	time_t now = time(nullptr);
	strftime(buffer, sizeof(buffer), "%T", localtime(&now));
	sys::cout << "AzAudio[" << buffer << "] ";
	va_list args;
	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);
	sys::cout << buffer;
}

#define NUM_CHANNELS 2

azaLookaheadLimiterData limiterData[NUM_CHANNELS] = {{}};
azaCompressorData compressorData[NUM_CHANNELS] = {{}};
azaDelayData delayData[NUM_CHANNELS] = {{}};
azaDelayData delay2Data[NUM_CHANNELS] = {{}};
azaDelayData delay3Data[NUM_CHANNELS] = {{}};
azaReverbData reverbData[NUM_CHANNELS] = {{}};
azaFilterData highPassData[NUM_CHANNELS] = {{}};
azaGateData gateData[NUM_CHANNELS] = {{}};
azaFilterData gateBandPass[NUM_CHANNELS] = {{}};
azaFilterData delayWetFilterData[NUM_CHANNELS] = {{}};

std::vector<float> micBuffer;
size_t lastMicBufferSize=0;
size_t lastInputBufferSize=0;
size_t numOutputBuffers=0;
size_t numInputBuffers=0;

azaChannelLayout outputChannelLayout = {0};
float angle = 0.0f;

int mixCallbackOutput(azaBuffer buffer, void *userData) {
	static float *processingBuffer = new float[2048];
	static size_t processingBufferCapacity = 2048;
	if (processingBufferCapacity < buffer.frames) {
		if (processingBuffer) delete[] processingBuffer;
		processingBufferCapacity = buffer.frames;
		processingBuffer = new float[processingBufferCapacity];
	}
	numOutputBuffers++;
	if (micBuffer.size() == lastMicBufferSize && micBuffer.size() > buffer.frames*2) {
		sys::cout << "Shrunk!" << std::endl;
		// Crossfade from new end to actual end
		float t = 0.0f;
		size_t crossFadeLen = std::min(micBuffer.size() - buffer.frames, 256ull);
		for (size_t i = micBuffer.size()-crossFadeLen; i < micBuffer.size(); i++) {
			t = std::min(1.0f, t + 1.0f / (float)crossFadeLen);
			float *dst = &micBuffer[i - buffer.frames];
			float src = micBuffer[i];
			*dst = *dst + (src - *dst) * t;
		}
		micBuffer.erase(micBuffer.end() - buffer.frames, micBuffer.end());
	}
	lastMicBufferSize = micBuffer.size();
	// printf("micBuffer size: %d\n", micBuffer.size() / buffer.channels);
	// printf("output has ");
	size_t i = 0;
	static float lastSample = 0.0f;
	static float fadein = 0.0f;
	for (; i < std::min((size_t)buffer.frames, micBuffer.size()); i++) {
		fadein = std::min(1.0f, fadein + 1.0f / 256.0f);
		lastSample = std::max(0.0f, lastSample - 1.0f / 256.0f);
		processingBuffer[i] = lastSample + micBuffer[i] * fadein;
	}
	if (buffer.frames > micBuffer.size()) {
		fadein = 0.0f;
		if (micBuffer.size()) lastSample = micBuffer.back();
		sys::cout << "Buffer underrun (" << micBuffer.size() << "/" << buffer.frames << " frames available, last input buffer was " << lastInputBufferSize << " samples and last output buffer was " << buffer.frames << " samples, had " << numInputBuffers << " input buffers and " << numOutputBuffers << " output buffers so far)" << std::endl;
	}
	micBuffer.erase(micBuffer.begin(), micBuffer.begin() + i);
	for (; i < buffer.frames; i++) {
		lastSample = std::max(0.0f, lastSample - 1.0f / 256.0f);
		processingBuffer[i] = lastSample;
	}
	int err;
	azaBuffer srcBuffer;
	srcBuffer.channels = 1;
	srcBuffer.frames = buffer.frames;
	srcBuffer.samplerate = buffer.samplerate;
	srcBuffer.samples = processingBuffer;
	srcBuffer.stride = 1;
	azaVec3 srcPosStart = {
		sin(angle),
		0.0f,
		cos(angle),
	};
	angle += (float)buffer.frames / (float)buffer.samplerate;
	if (angle > AZA_TAU) {
		angle -= AZA_TAU;
	}
	azaVec3 srcPosEnd = {
		sin(angle),
		0.0f,
		cos(angle),
	};
	memset(buffer.samples, 0, buffer.frames * buffer.channels * sizeof(float));
	azaMixChannelsSimple(buffer, outputChannelLayout, srcBuffer, srcPosStart, 1.0f, srcPosEnd, 1.0f, nullptr);
	// if ((err = azaFilter(buffer, delayWetFilterData))) {
	// 	return err;
	// }
	// if ((err = azaGate(buffer, gateData))) {
	// 	return err;
	// }
	// printf("gate gain: %f\n", gateData->gain);
	// if ((err = azaDelay(buffer, delayData))) {
	// 	return err;
	// }
	// if ((err = azaDelay(buffer, delay2Data))) {
	// 	return err;
	// }
	// if ((err = azaDelay(buffer, delay3Data))) {
	// 	return err;
	// }
	// if ((err = azaReverb(buffer, reverbData))) {
	// 	return err;
	// }
	// if ((err = azaFilter(buffer, highPassData))) {
	// 	return err;
	// }
	// if ((err = azaCompressor(buffer, compressorData))) {
	// 	return err;
	// }
	if ((err = azaLookaheadLimiter(buffer, limiterData))) {
		return err;
	}
	return AZA_SUCCESS;
}

int mixCallbackInput(azaBuffer buffer, void *userData) {
	numInputBuffers++;
	lastInputBufferSize = buffer.frames;
	size_t b_i = micBuffer.size();
	micBuffer.resize(micBuffer.size() + buffer.frames);
	for (unsigned long i = 0; i < buffer.frames; i++) {
		micBuffer[b_i + i] = buffer.samples[i];
	}
	return AZA_SUCCESS;
}

int main(int argumentCount, char** argumentValues) {
	using a_fit = std::runtime_error;
	#ifdef __unix
	signal(SIGSEGV, handler);
	#endif
	try {
		azaSetLogCallback(logCallback);
		int err = azaInit();
		if (err) {
			throw a_fit("Failed to azaInit!");
		}
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
		for (int c = 0; c < NUM_CHANNELS; c++) {
			gateData[c].threshold = -42.0f;
			gateData[c].attack = 10.0f;
			gateData[c].decay = 500.0f;
			azaGateDataInit(&gateData[c]);
			gateData[c].activationEffects = (azaDSPData*)&gateBandPass[c];

			gateBandPass[c].kind = AZA_FILTER_BAND_PASS;
			gateBandPass[c].frequency = 300.0f;
			azaFilterDataInit(&gateBandPass[c]);

			delayData[c].gain = -15.0f;
			delayData[c].gainDry = 0.0f;
			delayData[c].delay = 1234.5f;
			delayData[c].feedback = 0.5f;
			azaDelayDataInit(&delayData[c]);

			delay2Data[c].gain = -15.0f;
			delay2Data[c].gainDry = 0.0f;
			delay2Data[c].delay = 2345.6f;
			delay2Data[c].feedback = 0.5f;
			azaDelayDataInit(&delay2Data[c]);

			delay3Data[c].gain = -15.0f;
			delay3Data[c].gainDry = 0.0f;
			delay3Data[c].delay = 1000.0f / 3.0f;
			delay3Data[c].feedback = 0.98f;
			azaDelayDataInit(&delay3Data[c]);
			delay3Data[c].wetEffects = (azaDSPData*)&delayWetFilterData[c];

			delayWetFilterData[c].kind = AZA_FILTER_BAND_PASS;
			delayWetFilterData[c].frequency = 800.0f;
			delayWetFilterData[c].dryMix = 0.5f;
			azaFilterDataInit(&delayWetFilterData[c]);

			highPassData[c].kind = AZA_FILTER_HIGH_PASS;
			highPassData[c].frequency = 50.0f;
			azaFilterDataInit(&highPassData[c]);

			reverbData[c].gain = -15.0f;
			reverbData[c].gainDry = 0.0f;
			reverbData[c].roomsize = 10.0f;
			reverbData[c].color = 0.5f;
			reverbData[c].delay = c * 377.0f / 48000.0f;
			azaReverbDataInit(&reverbData[c]);

			compressorData[c].threshold = -24.0f;
			compressorData[c].ratio = 10.0f;
			compressorData[c].attack = 100.0f;
			compressorData[c].decay = 200.0f;
			azaCompressorDataInit(&compressorData[c]);

			limiterData[c].gainInput = 24.0f;
			limiterData[c].gainOutput = -6.0f;
			azaLookaheadLimiterDataInit(&limiterData[c]);
		}
		azaStream streamInput = {0};
		streamInput.mixCallback = mixCallbackInput;
		streamInput.deviceInterface = AZA_INPUT;
		streamInput.channels = 1;
		// streamInput.samplerate = 44100/4;
		if (azaStreamInit(&streamInput) != AZA_SUCCESS) {
			throw a_fit("Failed to init input stream!");
		}
		// This ensures that if anything changes in the backend, our input stream will always have the same channels and samplerate.
		// TODO: Make this a flag somewhere, perhaps in azaStreamInit
		streamInput.channels = azaStreamGetChannels(&streamInput);
		streamInput.samplerate = azaStreamGetSamplerate(&streamInput);
		azaStream streamOutput = {0};
		streamOutput.channels = NUM_CHANNELS;
		streamOutput.samplerate = streamInput.samplerate;
		streamOutput.mixCallback = mixCallbackOutput;
		if (azaStreamInit(&streamOutput) != AZA_SUCCESS) {
			throw a_fit("Failed to init output stream!");
		}
		outputChannelLayout = azaStreamGetChannelLayout(&streamOutput);
		std::cout << "Press ENTER to stop" << std::endl;
		std::cin.get();
		azaStreamDeinit(&streamInput);
		azaStreamDeinit(&streamOutput);
		for (int c = 0; c < NUM_CHANNELS; c++) {
			azaDelayDataDeinit(&delayData[c]);
			azaReverbDataDeinit(&reverbData[c]);
		}

		azaDeinit();
	} catch (std::runtime_error& e) {
		sys::cout << "Runtime Error: " << e.what() << std::endl;
	}
	return 0;
}

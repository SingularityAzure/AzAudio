/*
	File: main.cpp
	Author: singularity
	Simple test program for our library
*/

#if defined(_MSC_VER)
	#define _CRT_USE_CONFORMING_ANNEX_K_TIME 1
#endif
#include <ctime>

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
	struct tm timeBuffer;
	strftime(buffer, sizeof(buffer), "%T", localtime_s(&now, &timeBuffer));
	sys::cout << "AzAudio[" << buffer << "] ";
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, 1024, format, args);
	va_end(args);
	sys::cout << buffer;
}

azaLookaheadLimiter *limiter = nullptr;
azaCompressor *compressor = nullptr;
azaDelay *delay = nullptr;
azaDelay *delay2 = nullptr;
azaDelay *delay3 = nullptr;
azaReverb *reverb = nullptr;
azaFilter *highPass = nullptr;
azaGate *gate = nullptr;
azaFilter *gateBandPass = nullptr;
azaFilter *delayWetFilter = nullptr;
azaDelayDynamic *delayDynamic = nullptr;
azaSpatialize *spatialize = nullptr;

std::vector<float> micBuffer;
size_t lastMicBufferSize=0;
size_t lastInputBufferSize=0;
size_t numOutputBuffers=0;
size_t numInputBuffers=0;

float angle = 0.0f;
float angle2 = 0.0f;
std::vector<float> endChannelDelays;

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
	srcBuffer.channels.count = 1;
	srcBuffer.frames = buffer.frames;
	srcBuffer.samplerate = buffer.samplerate;
	srcBuffer.samples = processingBuffer;
	srcBuffer.stride = 1;
	// float distance = (0.5f + 0.5f * sin(angle2)) * 100.0f;
	azaVec3 srcPosStart = {
		sin(angle) * 100.0f,
		10.0f,
		0.0f,
		// 0.0f,
	};
	angle += ((float)buffer.frames / (float)buffer.samplerate) * AZA_TAU * 0.05f;
	// angle2 += ((float)buffer.frames / (float)buffer.samplerate) * AZA_TAU * 1.0f;
	if (angle > AZA_TAU) {
		angle -= AZA_TAU;
	}
	// distance = (0.5f + 0.5f * sin(angle2)) * 100.0f;
	azaVec3 srcPosEnd = {
		sin(angle) * 100.0f,
		10.0f,
		// cos(angle) * distance,
		0.0f,
		// 0.0f,
	};
	if ((err = azaProcessGate(srcBuffer, gate))) {
		return err;
	}
	memset(buffer.samples, 0, buffer.frames * buffer.channels.count * sizeof(float));
	float volumeStart = azaClampf(10.0f / azaVec3Norm(srcPosStart), 0.0f, 1.0f);
	float volumeEnd = azaClampf(10.0f / azaVec3Norm(srcPosEnd), 0.0f, 1.0f);
	if ((err = azaProcessSpatialize(spatialize, buffer, srcBuffer, srcPosStart, volumeStart, srcPosEnd, volumeEnd))) {
		return err;
	}
	// printf("gate gain: %f\n", gate->gain);
	endChannelDelays.resize(buffer.channels.count);
	for (float &delay : endChannelDelays) {
		delay = 600.0f + sinf(angle2) * 400.0f;
	}
	// if ((err = azaProcessDelayDynamic(buffer, delayDynamic, endChannelDelays.data()))) {
	// 	return err;
	// }
	// if ((err = azaProcessDelay(buffer, delay))) {
	// 	return err;
	// }
	// if ((err = azaProcessDelay(buffer, delay2))) {
	// 	return err;
	// }
	// if ((err = azaProcessDelay(buffer, delay3))) {
	// 	return err;
	// }
	if ((err = azaProcessReverb(buffer, reverb))) {
		return err;
	}
	if ((err = azaProcessFilter(buffer, highPass))) {
		return err;
	}
	if ((err = azaProcessCompressor(buffer, compressor))) {
		return err;
	}
	if ((err = azaProcessLookaheadLimiter(buffer, limiter))) {
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
		// Change world to z-up
		azaWorldDefault.orientation.right = azaVec3 { 1.0f, 0.0f, 0.0f };
		azaWorldDefault.orientation.up = azaVec3 { 0.0f, 0.0f, 1.0f };
		azaWorldDefault.orientation.forward = azaVec3 { 0.0f, 1.0f, 0.0f };
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
		azaStream streamInput = {0};
		streamInput.mixCallback = mixCallbackInput;
		streamInput.deviceInterface = AZA_INPUT;
		streamInput.channels = azaChannelLayoutMono();
		// streamInput.samplerate = 44100/4;
		if (azaStreamInit(&streamInput) != AZA_SUCCESS) {
			throw a_fit("Failed to init input stream!");
		}
		// This ensures that if anything changes in the backend, our input stream will always have the same channels and samplerate.
		// TODO: Make this a flag somewhere, perhaps in azaStreamInit
		streamInput.channels = azaStreamGetChannelLayout(&streamInput);
		streamInput.samplerate = azaStreamGetSamplerate(&streamInput);
		azaStream streamOutput = {0};
		// streamOutput.channels = azaChannelLayoutStandardFromCount(NUM_CHANNELS);
		streamOutput.samplerate = streamInput.samplerate;
		streamOutput.mixCallback = mixCallbackOutput;
		if (azaStreamInit(&streamOutput) != AZA_SUCCESS) {
			throw a_fit("Failed to init output stream!");
		}
		uint8_t outputChannelCount = azaStreamGetChannelLayout(&streamOutput).count;
		// Configure all the DSP functions
		// gate runs on the single-channel mic buffer
		gateBandPass = azaMakeFilter(azaFilterConfig{
			/* .kind      = */ AZA_FILTER_BAND_PASS,
			/* .frequency = */ 300.0f,
			/* .dryMix    = */ 0.0f,
		}, 1);

		gate = azaMakeGate(azaGateConfig{
			/* .threshold         = */-42.0f,
			/* .attack            = */ 10.0f,
			/* .decay             = */ 500.0f,
			/* .activationEffects = */ (azaDSP*)gateBandPass,
		});

		delay = azaMakeDelay(azaDelayConfig{
			/* .gain       = */-15.0f,
			/* .gainDry    = */ 0.0f,
			/* .delay      = */ 1234.5f,
			/* .feedback   = */ 0.5f,
			/* .pingpong   = */ 0.9f,
			/* .wetEffects = */ nullptr,
		}, outputChannelCount);

		delay2 = azaMakeDelay(azaDelayConfig{
			/* .gain       = */-15.0f,
			/* .gainDry    = */ 0.0f,
			/* .delay      = */ 2345.6f,
			/* .feedback   = */ 0.5f,
			/* .pingpong   = */ 0.2f,
			/* .wetEffects = */ nullptr,
		}, outputChannelCount);

		delayWetFilter = azaMakeFilter(azaFilterConfig{
			/* .kind      = */ AZA_FILTER_BAND_PASS,
			/* .frequency = */ 800.0f,
			/* .dryMix    = */ 0.5f,
		}, outputChannelCount);

		delay3 = azaMakeDelay(azaDelayConfig{
			/* .gain       = */-15.0f,
			/* .gainDry    = */ 0.0f,
			/* .delay      = */ 1000.0f / 3.0f,
			/* .feedback   = */ 0.98f,
			/* .pingpong   = */ 0.0f,
			/* .wetEffects = */ (azaDSP*)delayWetFilter,
		}, outputChannelCount);

		highPass = azaMakeFilter(azaFilterConfig{
			/* .kind      = */ AZA_FILTER_HIGH_PASS,
			/* .frequency = */ 50.0f,
			/* .dryMix    = */ 0.0f,
		}, outputChannelCount);

		reverb = azaMakeReverb(azaReverbConfig{
			/* .gain     = */-15.0f,
			/* .gainDry  = */ 0.0f,
			/* .roomsize = */ 100.0f,
			/* .color    = */ 1.0f,
			/* .delay    = */ 200.0f,
		}, outputChannelCount);
		// TODO: maybe recreate this? reverbData[c].delay = c * 377.0f / 48000.0f;

		compressor = azaMakeCompressor(azaCompressorConfig{
			/* .threshold = */-24.0f,
			/* .ratio     = */ 10.0f,
			/* .attack    = */ 100.0f,
			/* .decay     = */ 200.0f,
		});

		limiter = azaMakeLookaheadLimiter(azaLookaheadLimiterConfig{
			/* .gainInput  = */ 24.0f,
			/* .gainOutput = */-6.0f,
		}, outputChannelCount);

		std::vector<azaDelayDynamicChannelConfig> channelDelays(outputChannelCount, azaDelayDynamicChannelConfig{
			/* .delay = */ 500.0f,
		});
		delayDynamic = azaMakeDelayDynamic(azaDelayDynamicConfig{
			/* .gain       = */ 0.0f,
			/* .gainDry    = */-INFINITY,
			/* .delayMax   = */ 1000.0f,
			/* .feedback   = */ 0.8f,
			/* .pingpong   = */ 0.0f,
			/* .wetEffects = */ (azaDSP*)delayWetFilter,
			/* .kernel     = */ nullptr,
		}, outputChannelCount, outputChannelCount, channelDelays.data());

		spatialize = azaMakeSpatialize(azaSpatializeConfig{
			/* .world       = */ nullptr,
			/* .mode        = */ AZA_SPATIALIZE_ADVANCED,
			/* .delayMax    = */ 0.0f,
			/* .earDistance = */ 0.0f,
		}, outputChannelCount);

		azaStreamSetActive(&streamInput, 1);
		azaStreamSetActive(&streamOutput, 1);
		std::cout << "Press ENTER to stop" << std::endl;
		std::cin.get();
		azaStreamDeinit(&streamInput);
		azaStreamDeinit(&streamOutput);

		azaFreeLookaheadLimiter(limiter);
		azaFreeCompressor(compressor);
		azaFreeDelay(delay);
		azaFreeDelay(delay2);
		azaFreeDelay(delay3);
		azaFreeReverb(reverb);
		azaFreeFilter(highPass);
		azaFreeGate(gate);
		azaFreeFilter(gateBandPass);
		azaFreeFilter(delayWetFilter);
		azaFreeDelayDynamic(delayDynamic);
		azaFreeSpatialize(spatialize);

		azaDeinit();
	} catch (std::runtime_error& e) {
		sys::cout << "Runtime Error: " << e.what() << std::endl;
	}
	return 0;
}

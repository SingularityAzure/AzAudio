/*
	File: AzAudio.c
	Author: Philip Haynes
*/

#include "AzAudio.h"

#include "error.h"
#include "helpers.h"
#include "backend/interface.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#if defined(_MSC_VER)
	// Michaelsoft will not be spared my wrath at the end of days
	#define _CRT_USE_CONFORMING_ANNEX_K_TIME 1
#endif
#include <time.h>

fp_azaLogCallback azaLog = azaLogDefault;

AzaLogLevel azaLogLevel = AZA_LOG_LEVEL_INFO;

azaAllocatorCallbacks azaAllocator = {0};

int azaInit() {
	char levelStr[64];
	size_t levelLen = 0;
	errno_t hasLogLevel = getenv_s(&levelLen, levelStr, 64, "AZAUDIO_LOG_LEVEL");
	if (0 == hasLogLevel && levelLen <= sizeof(levelStr)) {
		azaStrToLower(levelStr, sizeof(levelStr), levelStr);
		if (strncmp(levelStr, "none", sizeof(levelStr)) == 0) {
			azaLogLevel = AZA_LOG_LEVEL_NONE;
		} else if (strncmp(levelStr, "error", sizeof(levelStr)) == 0) {
			azaLogLevel = AZA_LOG_LEVEL_ERROR;
		} else if (strncmp(levelStr, "info", sizeof(levelStr)) == 0) {
			azaLogLevel = AZA_LOG_LEVEL_INFO;
		} else if (strncmp(levelStr, "trace", sizeof(levelStr)) == 0) {
			azaLogLevel = AZA_LOG_LEVEL_TRACE;
		}
	}
	AZA_LOG_INFO("AzAudio Version: " AZA_VERSION_FORMAT_STR "\n", AZA_VERSION_ARGS);

	if (azaAllocator.fp_calloc == NULL) {
		azaAllocator.fp_calloc = calloc;
		azaAllocator.fp_malloc = malloc;
		azaAllocator.fp_free = free;
	}

	azaKernelMakeLanczos(&azaKernelDefaultLanczos, 128.0f, 50.0f);

	memset(&azaWorldDefault, 0, sizeof(azaWorldDefault));
	azaWorldDefault.orientation.right   = (azaVec3) { 1.0f, 0.0f, 0.0f };
	azaWorldDefault.orientation.up      = (azaVec3) { 0.0f, 1.0f, 0.0f };
	azaWorldDefault.orientation.forward = (azaVec3) { 0.0f, 0.0f, 1.0f };
	azaWorldDefault.speedOfSound = 343.0f;
	return azaBackendInit();
}

void azaDeinit() {
	azaBackendDeinit();
}

void azaLogDefault(AzaLogLevel level, const char* format, ...) {
	if (level > azaLogLevel) return;
	FILE *file = level == AZA_LOG_LEVEL_ERROR ? stderr : stdout;
	char timeStr[64];
	struct tm timeBuffer;
	strftime(timeStr, sizeof(timeStr), "%T", localtime_s(&(time_t){time(NULL)}, &timeBuffer));
	fprintf(file, "AzAudio[%s] ", timeStr);
	va_list args;
	va_start(args, format);
	vfprintf(file, format, args);
	va_end(args);
}

void azaSetLogCallback(fp_azaLogCallback newLogFunc) {
	if (newLogFunc != NULL) {
		azaLog = newLogFunc;
	} else {
		azaLog = azaLogDefault;
	}
}

const char *azaErrorStr[] = {
	"AZA_SUCCESS",
	"AZA_ERROR_BACKEND_UNAVAILABLE",
	"AZA_ERROR_BACKEND_LOAD_ERROR",
	"AZA_ERROR_BACKEND_ERROR",
	"AZA_ERROR_NO_DEVICES_AVAILABLE",
	"AZA_ERROR_NULL_POINTER",
	"AZA_ERROR_INVALID_CHANNEL_COUNT",
	"AZA_ERROR_CHANNEL_COUNT_MISMATCH",
	"AZA_ERROR_INVALID_FRAME_COUNT",
	"AZA_ERROR_INVALID_CONFIGURATION",
	"AZA_ERROR_INVALID_DSP_STRUCT",
};

const char* azaErrorString(int error, char *buffer, size_t bufferSize) {
	if (0 <= error && error < AZA_ERROR_ONE_AFTER_LAST) {
		return azaErrorStr[error];
	}
	if (buffer && bufferSize) {
		snprintf(buffer, bufferSize, "Unknown Error 0x%x", error);
		return buffer;
	}
	return "No buffer for unknown error code :(";
}
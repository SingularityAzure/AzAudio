/*
	File: AzAudio.c
	Author: Philip Haynes
*/

#include "AzAudio.h"

#include "error.h"
#include "helpers.h"
#include "backend/interface.h"

#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

fp_azaLogCallback azaLog = azaLogDefault;

AzaLogLevel azaLogLevel = AZA_LOG_LEVEL_INFO;

int azaInit() {
	const char *envLogLevel = getenv("AZAUDIO_LOG_LEVEL");
	if (envLogLevel) {
		char level[64];
		azaStrToLower(level, sizeof(level), envLogLevel);
		if (strncmp(level, "none", sizeof(level)) == 0) {
			azaLogLevel = AZA_LOG_LEVEL_NONE;
		} else if (strncmp(level, "error", sizeof(level)) == 0) {
			azaLogLevel = AZA_LOG_LEVEL_ERROR;
		} else if (strncmp(level, "info", sizeof(level)) == 0) {
			azaLogLevel = AZA_LOG_LEVEL_INFO;
		} else if (strncmp(level, "trace", sizeof(level)) == 0) {
			azaLogLevel = AZA_LOG_LEVEL_TRACE;
		}
	}
	AZA_LOG_INFO("AzAudio Version: " AZA_VERSION_FORMAT_STR "\n", AZA_VERSION_ARGS);

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
	strftime(timeStr, sizeof(timeStr), "%T", localtime(&(time_t){time(NULL)}));
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
	"AZA_ERROR_NO_DEVICE_AVAILABLE",
	"AZA_ERROR_NULL_POINTER",
	"AZA_ERROR_INVALID_CHANNEL_COUNT",
	"AZA_ERROR_INVALID_FRAME_COUNT",
	"AZA_ERROR_INVALID_CONFIGURATION",
	"AZA_ERROR_INVALID_DSP_STRUCT",
};
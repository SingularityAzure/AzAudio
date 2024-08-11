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

// Helper functions

fp_azaLogCallback azaPrint = azaDefaultLogFunc;

int azaInit() {
	return azaBackendInit();
}

void azaDeinit() {
	azaBackendDeinit();
}

void azaDefaultLogFunc(const char* message) {
	#ifndef AZAUDIO_NO_STDIO
	printf("AzAudio: %s\n",message);
	#endif
}

void azaSetLogCallback(fp_azaLogCallback newLogFunc) {
	if (newLogFunc != NULL) {
		azaPrint = newLogFunc;
	} else {
		azaPrint = azaDefaultLogFunc;
	}
}

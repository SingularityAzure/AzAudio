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

fp_azaLogCallback azaPrint;

int azaInit() {
	int err;
	azaPrint = azaDefaultLogFunc;
	err = azaBackendInit();
	if (err) return err;
	
	return AZA_SUCCESS;
}

void azaDeinit() {
	azaBackendDeinit();
}

void azaDefaultLogFunc(const char* message) {
	#ifndef AZAUDIO_NO_STDIO
	printf("AzAudio: %s\n",message);
	#endif
}

int azaSetLogCallback(fp_azaLogCallback newLogFunc) {
	if (newLogFunc != NULL) {
		azaPrint = newLogFunc;
	} else {
		azaPrint = azaDefaultLogFunc;
	}
	return AZA_SUCCESS;
}

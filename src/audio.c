/*
    File: audio.c
    Author: singularity
*/

#include "audio.h"

#ifndef SYS_AUDIO_NO_STDIO
#include <stdio.h>
#endif

int azaError;

void azaDefaultLogFunc(const char* message) {
    #ifndef SYS_AUDIO_NO_STDIO
    printf("%s\n",message);
    #endif
}

fpLogCallback azaPrintl = azaDefaultLogFunc;

int azaSetLogCallback(fpLogCallback newLogFunc) {
    if (newLogFunc != NULL) {
        azaPrintl = newLogFunc;
        azaError = AZA_SUCCESS;
    } else {
        azaError = AZA_ERROR_NULL_POINTER;
    }
    return azaError;
}

void azaHelloWorld() {
    azaPrintl("Hello World!");
}

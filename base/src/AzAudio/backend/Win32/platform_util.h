#ifndef AZAUDIO_WIN32_PLATFORM_UTIL_H
#define AZAUDIO_WIN32_PLATFORM_UTIL_H

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

const char* HRESULT_String(HRESULT hResult);

#ifdef __cplusplus
}
#endif

#endif // AZAUDIO_WIN32_PLATFORM_UTIL_H
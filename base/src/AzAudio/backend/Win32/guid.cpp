/*
	File: guid.cpp
	Author: Philip Haynes
	Because Microsoft doesn't define these anywhere for some reason...
*/

#include <initguid.h>
#include <Mmdeviceapi.h>
#include <ks.h>
#include <ksmedia.h>
#include <Audioclient.h>

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IMMEndpoint = __uuidof(IMMEndpoint);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

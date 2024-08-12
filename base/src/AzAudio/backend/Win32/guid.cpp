/*
	File: guid.cpp
	Author: Philip Haynes
	Because Microsoft doesn't define these anywhere for some reason...
*/

#include <Mmdeviceapi.h>

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
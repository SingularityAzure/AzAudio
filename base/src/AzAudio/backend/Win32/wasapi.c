/*
	File: wasapi.c
	Author: Philip Haynes
*/

#include "../backend.h"
#include "../interface.h"
#include "../../error.h"
#include "../../helpers.h"

#include <Mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <stringapiset.h>

// Microsoft APIs are really stinky and smelly and dirty and leaving their dirt all over everything they touch.
#ifdef interface
#undef interface
#endif

// Don't forget to free() the return value later.
static char* wstrToCstr(WCHAR *wstr) {
	char *cstr;
	int neededSize = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
	cstr = malloc(neededSize);
	WideCharToMultiByte(CP_UTF8, 0, wstr, -1, cstr, neededSize, NULL, NULL);
	return cstr;
}

static IMMDeviceEnumerator *pEnumerator = NULL;
static IMMDeviceCollection *pDeviceCollection = NULL;

#define AZA_MAX_DEVICES 256

typedef struct azaDeviceInfo {
	IMMDevice *pDevice;
	// I'm buying clothes at the Property Store
	IPropertyStore *pPropertyStore;
	const WCHAR *idWStr;
	const char *name;
	unsigned channels;
} azaDeviceInfo;

static azaDeviceInfo device[AZA_MAX_DEVICES];
static size_t deviceCount = 0;

static azaDeviceInfo deviceOutput[AZA_MAX_DEVICES];
static size_t deviceOutputCount = 0;
static azaDeviceInfo deviceInput[AZA_MAX_DEVICES];
static size_t deviceInputCount = 0;

static azaDeviceInfo defaultOutputDevice = {0};
static azaDeviceInfo defaultInputDevice = {0};

#define CHECK_RESULT(description) if (FAILED(result)) {\
	AZA_PRINT_ERR(description " failed:%ld\n", result);\
	goto error;\
}
#define SAFE_RELEASE(pSomething) if ((pSomething)) { (pSomething)->lpVtbl->Release((pSomething)); (pSomething) = NULL; }
#define SAFE_FREE(pSomething) if ((pSomething)) { free((pSomething)); (pSomething) = NULL; }

static void azaWASAPIDeinit() {
	SAFE_RELEASE(pEnumerator);
	SAFE_RELEASE(pDeviceCollection);
	for (size_t i = 0; i < deviceCount; i++) {
		SAFE_RELEASE(device[i].pDevice);
		SAFE_RELEASE(device[i].pPropertyStore);
		CoTaskMemFree(device[i].idWStr);
		SAFE_FREE(device[i].name);
	}
}

static int azaWASAPIInit() {
	HRESULT result;
	result = CoInitialize(NULL);
	CHECK_RESULT("CoInitialize");
	result = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, &pEnumerator);
	CHECK_RESULT("CoCreateInstance");
	result = pEnumerator->lpVtbl->GetDefaultAudioEndpoint(pEnumerator, eRender, eConsole, &defaultOutputDevice.pDevice);
	CHECK_RESULT("GetDefaultAudioEndpoint (render)");
	result = pEnumerator->lpVtbl->GetDefaultAudioEndpoint(pEnumerator, eCapture, eConsole, &defaultInputDevice.pDevice);
	CHECK_RESULT("GetDefaultAudioEndpoint (capture)");

	result = pEnumerator->lpVtbl->EnumAudioEndpoints(pEnumerator, eAll, DEVICE_STATE_ACTIVE, &pDeviceCollection);
	CHECK_RESULT("EnumAudioEndpoints");
	UINT deviceCountUINT = 0;
	pDeviceCollection->lpVtbl->GetCount(pDeviceCollection, &deviceCountUINT);
	if (deviceCountUINT > AZA_MAX_DEVICES) {
		AZA_PRINT_INFO("System has too many devices (%u)! Only enumerating the first %u...\n", deviceCountUINT, AZA_MAX_DEVICES);
		deviceCountUINT = AZA_MAX_DEVICES;
	}
	deviceCount = (size_t)deviceCountUINT;
	for (UINT i = 0; i < deviceCountUINT; i++) {
		IMMDevice *pDevice;
		IPropertyStore *pPropertyStore;
		WCHAR *idWStr;
		char *name;
		pDeviceCollection->lpVtbl->Item(pDeviceCollection, i, &pDevice);
		pDevice->lpVtbl->GetId(pDevice, &idWStr);

		result = pDevice->lpVtbl->OpenPropertyStore(pDevice, STGM_READ, &pPropertyStore);
		CHECK_RESULT("OpenPropertyStore");

		PROPVARIANT varName;
		PropVariantInit(&varName);
		result = pPropertyStore->lpVtbl->GetValue(pPropertyStore, &PKEY_Device_FriendlyName, &varName);
		CHECK_RESULT("PropertyStore::GetValue");
		if (varName.vt != VT_EMPTY) {
			name = wstrToCstr(varName.pwszVal);
		}
		char *idCStr = wstrToCstr(idWStr);
		AZA_PRINT_INFO("Device %u: idStr:\"%s\" name:\"%s\"\n", i, idCStr, name);
		free(idCStr);

		device[i].pDevice = pDevice;
		device[i].pPropertyStore = pPropertyStore;
		device[i].idWStr = idWStr;
		device[i].name = name;
	}
	return AZA_SUCCESS;
error:
	azaWASAPIDeinit();
	return AZA_ERROR_BACKEND_LOAD_ERROR;
}

static int azaStreamInitWASAPI(azaStream *stream, const char *device) {
	return AZA_ERROR_BACKEND_ERROR;
}

static void azaStreamDeinitWASAPI(azaStream *stream) {

}

static size_t azaGetDeviceCountWASAPI(azaDeviceInterface interface) {
	switch (interface) {
		case AZA_OUTPUT: return deviceOutputCount;
		case AZA_INPUT: return deviceInputCount;
		default: return 0;
	}
}

static const char* azaGetDeviceNameWASAPI(azaDeviceInterface interface, size_t index) {
	return "Not telling >:(";
}

static size_t azaGetDeviceChannelsWASAPI(azaDeviceInterface interface, size_t index) {
	return 0;
}

int azaBackendWASAPIInit() {
	azaStreamInit = azaStreamInitWASAPI;
	azaStreamDeinit = azaStreamDeinitWASAPI;
	azaGetDeviceCount = azaGetDeviceCountWASAPI;
	azaGetDeviceName = azaGetDeviceNameWASAPI;
	azaGetDeviceChannels = azaGetDeviceChannelsWASAPI;

	return azaWASAPIInit();
}

void azaBackendWASAPIDeinit() {
	azaWASAPIDeinit();
}
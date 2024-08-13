/*
	File: wasapi.c
	Author: Philip Haynes
*/

#include "../backend.h"
#include "../interface.h"
#include "../../error.h"
#include "../../helpers.h"

#include <Mmdeviceapi.h>
#include <mmreg.h>
#include <functiondiscoverykeys_devpkey.h>
#include <Ks.h>
#include <Ksmedia.h>
#include <stringapiset.h>

// Microsoft APIs are really stinky and smelly and dirty and leaving their dirt all over everything they touch.
#ifdef interface
#undef interface
#endif

// This thing is horrible to look at, but works a treat!
#define GUID_PRINTF_FORMAT_STR "%08x-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"
#define GUID_PRINTF_ARGS(guid) (guid).Data1, (guid).Data2, (guid).Data3, (guid).Data4[0], (guid).Data4[1], (guid).Data4[2], (guid).Data4[3], (guid).Data4[4], (guid).Data4[5], (guid).Data4[6], (guid).Data4[7]

#define CHECK_RESULT(description) if (FAILED(result)) {\
	AZA_PRINT_ERR(description " failed:%ld\n", result);\
	goto error;\
}
#define SAFE_RELEASE(pSomething) if ((pSomething)) { (pSomething)->lpVtbl->Release((pSomething)); (pSomething) = NULL; }
#define SAFE_FREE(pSomething) if ((pSomething)) { free((pSomething)); (pSomething) = NULL; }

// Don't forget to free() the return value later.
static char* wstrToCstr(WCHAR *wstr) {
	char *cstr;
	int neededSize = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
	cstr = malloc(neededSize);
	WideCharToMultiByte(CP_UTF8, 0, wstr, -1, cstr, neededSize, NULL, NULL);
	return cstr;
}

#define PROPERTY_STORE_GET(pPropertyStore, key, VT_KIND, onSuccess, onFail) {\
	PROPVARIANT propVariant;\
	PropVariantInit(&propVariant);\
	result = (pPropertyStore)->lpVtbl->GetValue((pPropertyStore), &(key), &propVariant);\
	CHECK_RESULT("PropertyStore::GetValue(" #key ")\n");\
	if (propVariant.vt == (VT_KIND)) {\
		onSuccess;\
	} else {\
		AZA_PRINT_ERR("PropertyStore::GetValue(" #key ") vt kind is not " #VT_KIND " (was %hu)\n", propVariant.vt);\
		PropVariantClear(&propVariant);\
		onFail;\
	}\
	PropVariantClear(&propVariant);\
}

static IMMDeviceEnumerator *pEnumerator = NULL;
static IMMDeviceCollection *pDeviceCollection = NULL;

#define AZA_MAX_DEVICES 256

typedef enum azaSampleFormat {
	AZA_SAMPLE_PCM=0,
	AZA_SAMPLE_FLOAT,
} azaSampleFormat;
static const char *azaSampleFormatStr[] = {
	"PCM",
	"FLOAT",
};

typedef struct azaDeviceInfo {
	IMMDevice *pDevice;
	// I'm buying clothes at the Property Store
	IPropertyStore *pPropertyStore;
	const WCHAR *idWStr;
	// Contains the mask of where the channels are physically positioned, such as SPEAKER_FRONT_LEFT etc.
	UINT speakerConfig;
	const char *name;
	unsigned channels;
	unsigned sampleBitDepth;
	size_t samplerate;
	azaSampleFormat sampleFormat;
} azaDeviceInfo;

static azaDeviceInfo device[AZA_MAX_DEVICES];
static size_t deviceCount = 0;

static azaDeviceInfo deviceOutput[AZA_MAX_DEVICES];
static size_t deviceOutputCount = 0;
static azaDeviceInfo deviceInput[AZA_MAX_DEVICES];
static size_t deviceInputCount = 0;

static azaDeviceInfo defaultOutputDevice = {0};
static azaDeviceInfo defaultInputDevice = {0};



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
		// unsigned speakerConfig;
		WAVEFORMATEXTENSIBLE waveFormatEx;
		pDeviceCollection->lpVtbl->Item(pDeviceCollection, i, &pDevice);
		device[i].pDevice = pDevice;
		pDevice->lpVtbl->GetId(pDevice, &idWStr);
		device[i].idWStr = idWStr;

		result = pDevice->lpVtbl->OpenPropertyStore(pDevice, STGM_READ, &pPropertyStore);
		CHECK_RESULT("OpenPropertyStore");
		device[i].pPropertyStore = pPropertyStore;
		PROPERTY_STORE_GET(pPropertyStore, PKEY_Device_FriendlyName, VT_LPWSTR,
			// onSuccess:
			name = wstrToCstr(propVariant.pwszVal),
			// onFail:
			continue
		);
		device[i].name = name;
		// PROPERTY_STORE_GET(pPropertyStore, PKEY_AudioEndpoint_PhysicalSpeakers, VT_UI4, speakerConfig = propVariant.uintVal, speakerConfig=0);
		// device[i].speakerConfig = speakerConfig;
		PROPERTY_STORE_GET(pPropertyStore, PKEY_AudioEngine_DeviceFormat, VT_BLOB,
			// onSuccess:
			memcpy(&waveFormatEx, propVariant.blob.pBlobData, AZA_MIN(sizeof(waveFormatEx), propVariant.blob.cbSize)),
			// onFail:
			continue
		);
		device[i].channels = (unsigned)waveFormatEx.Format.nChannels;
		device[i].sampleBitDepth = (unsigned)waveFormatEx.Format.wBitsPerSample;
		device[i].samplerate = (size_t)waveFormatEx.Format.nSamplesPerSec;
		switch (waveFormatEx.Format.wFormatTag) {
			case WAVE_FORMAT_EXTENSIBLE:
				device[i].speakerConfig = waveFormatEx.dwChannelMask;
				if (IsEqualGUID(&waveFormatEx.SubFormat, &KSDATAFORMAT_SUBTYPE_PCM)) {
					device[i].sampleFormat = AZA_SAMPLE_PCM;
				} else if (IsEqualGUID(&waveFormatEx.SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
					device[i].sampleFormat = AZA_SAMPLE_FLOAT;
				} else {
					AZA_PRINT_ERR("Unhandled WAVEFORMATEXTENSIBLE::SubFormat " GUID_PRINTF_FORMAT_STR "\n", GUID_PRINTF_ARGS(waveFormatEx.SubFormat));
					continue;
				}
				break;
			case WAVE_FORMAT_PCM:
				device[i].speakerConfig = 0;
				device[i].sampleFormat = AZA_SAMPLE_PCM;
				break;
			case WAVE_FORMAT_IEEE_FLOAT:
				device[i].speakerConfig = 0;
				device[i].sampleFormat = AZA_SAMPLE_FLOAT;
				break;
			default:
				AZA_PRINT_ERR("Unhandled WAVEFORMATEX::wFormatTag %hu\n", waveFormatEx.Format.wFormatTag);
				continue;
		}

		char *idCStr = wstrToCstr(idWStr);
		AZA_PRINT_INFO("Device %u:\n\tidStr: \"%s\"\n\tname: \"%s\"\n\tchannels: %u\n\tbitDepth: %u\n\tsamplerate: %zu\n\tspeakerConfig: 0x%x\n", i, idCStr, name, device[i].channels, device[i].sampleBitDepth, device[i].samplerate, device[i].speakerConfig);
		free(idCStr);

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
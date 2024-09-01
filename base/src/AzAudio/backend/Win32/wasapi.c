/*
	File: wasapi.c
	Author: Philip Haynes
*/

#include "../backend.h"
#include "../interface.h"
#include "../../error.h"
#include "../../helpers.h"

#include "threads.h"

#include <Mmdeviceapi.h>
#include <mmreg.h>
#include <functiondiscoverykeys_devpkey.h>
#include <Ks.h>
#include <Ksmedia.h>
#include <stringapiset.h>
#include <Audioclient.h>

#include <assert.h>

// Microsoft APIs are really stinky and smelly and dirty and leaving their dirt all over everything they touch.
#ifdef interface
#undef interface
#endif

// This thing is horrible to look at, but works a treat!
#define GUID_FORMAT_STR "%08x-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"
#define GUID_ARGS(guid) (guid).Data1, (guid).Data2, (guid).Data3, (guid).Data4[0], (guid).Data4[1], (guid).Data4[2], (guid).Data4[3], (guid).Data4[4], (guid).Data4[5], (guid).Data4[6], (guid).Data4[7]

#define CHECK_RESULT(description, onFail) if (FAILED(hResult)) {\
	AZA_LOG_ERR(description " failed:%ld\n", hResult);\
	onFail;\
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
	hResult = (pPropertyStore)->lpVtbl->GetValue((pPropertyStore), &(key), &propVariant);\
	CHECK_RESULT("PropertyStore::GetValue(" #key ")\n", onFail);\
	if (propVariant.vt == (VT_KIND)) {\
		onSuccess;\
		PropVariantClear(&propVariant);\
	} else {\
		AZA_LOG_ERR("PropertyStore::GetValue(" #key ") vt kind is not " #VT_KIND " (was %hu)\n", propVariant.vt);\
		PropVariantClear(&propVariant);\
		onFail;\
	}\
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

static azaThread thread = {0};
static azaMutex mutex = {0};
static int shouldQuit = 0;

static azaDeviceInfo device[AZA_MAX_DEVICES];
static size_t deviceCount = 0;

static azaDeviceInfo deviceOutput[AZA_MAX_DEVICES];
static size_t deviceOutputCount = 0;
static azaDeviceInfo deviceInput[AZA_MAX_DEVICES];
static size_t deviceInputCount = 0;

static size_t defaultOutputDevice = AZA_MAX_DEVICES;
static size_t defaultInputDevice = AZA_MAX_DEVICES;

// TODO: Implement IMMNotificationClient to handle changes to audio devices

typedef struct azaStreamData {
	int isActive;
	azaStream *stream;
	azaDeviceInfo *deviceInfo;
	uint32_t bufferFrames;
	uint8_t *buffer;
	// If our exact format matches, then this will be NULL, and you can just use buffer. Otherwise, we need to convert formats, and this serves as our processing buffer.
	float *myBuffer;

	IAudioClient *pAudioClient;
	union {
		IAudioRenderClient *pRenderClient;
		IAudioCaptureClient *pCaptureClient;
	};
	WAVEFORMATEXTENSIBLE waveFormatExtensible;
} azaStreamData;

// This is a ridiculous amount of streams. Getting anywhere close to this is PROBABLY a misuse of this API.
#define AZA_MAX_STREAMS 64

static azaStreamData streams[AZA_MAX_STREAMS];
static size_t streamCount;

static azaStreamData* azaStreamDataGet() {
	azaStreamData *data = NULL;
	for (int i = 0; i < AZA_MAX_STREAMS; i++) {
		if (!streams[i].isActive) {
			data = &streams[i];
		}
	}
	streamCount++;
	memset(data, 0, sizeof(*data));
	return data;
}

static void azaStreamDataFree(azaStreamData *data) {
	if (data->isActive) {
		data->isActive = 0;
		streamCount--;
		SAFE_FREE(data->myBuffer);
	}
}


static void azaStreamProcess(azaStreamData *data) {
	HRESULT hResult;
	if (!data->isActive) return;

	uint32_t numFrames;

	azaStream *stream = data->stream;
	if (stream->deviceInterface == AZA_OUTPUT) {
		uint32_t numFramesUnread;
		hResult = data->pAudioClient->lpVtbl->GetCurrentPadding(data->pAudioClient, &numFramesUnread);
		numFrames = data->bufferFrames - numFramesUnread;
		if (numFrames == 0) return;
		AZA_LOG_TRACE("Processing %u output frames\n", numFrames);
		hResult = data->pRenderClient->lpVtbl->GetBuffer(data->pRenderClient, numFrames, &data->buffer);
		CHECK_RESULT("IAudioRenderClient::GetBuffer", return);
	} else {
		hResult = data->pCaptureClient->lpVtbl->GetNextPacketSize(data->pCaptureClient, &numFrames);
		if (hResult == AUDCLNT_E_DEVICE_INVALIDATED) {
			// TODO: Handle this
			return;
		}
		CHECK_RESULT("IAudioCaptureClient::GetNextPacketSize", return);
		if (numFrames == 0) return;
		DWORD flags;
		hResult = data->pCaptureClient->lpVtbl->GetBuffer(data->pCaptureClient, &data->buffer, &numFrames, &flags, NULL, NULL);
		CHECK_RESULT("IAudioCaptureClient::GetBuffer", return);
		AZA_LOG_TRACE("Processing %u input frames\n", numFrames);
		if (data->myBuffer) {
			// TODO: Convert the data format
			assert(0);
		}
	}

	float *samples = data->myBuffer ? data->myBuffer : (float*)data->buffer;

	stream->mixCallback((azaBuffer){
		.channels = stream->channels,
		.frames = numFrames,
		.samplerate = stream->samplerate,
		.samples = samples,
		.stride = stream->channels,
	}, stream->userdata);

	if (stream->deviceInterface == AZA_OUTPUT) {
		if (data->myBuffer) {
			// TODO: Convert the data format
			assert(0);
		}
		hResult = data->pRenderClient->lpVtbl->ReleaseBuffer(data->pRenderClient, numFrames, 0);
		CHECK_RESULT("IAudioRenderClient::ReleaseBuffer", return);
	} else {
		hResult = data->pCaptureClient->lpVtbl->ReleaseBuffer(data->pCaptureClient, numFrames);
		CHECK_RESULT("IAudioCaptureClient::ReleaseBuffer", return);
	}
}


static unsigned __stdcall soundThreadProc(void *userdata) {
	HRESULT hResult;
	hResult = CoInitialize(NULL);
	CHECK_RESULT("soundThreadProc CoInitialize", goto error);

	azaMutexLock(&mutex);
	while (!shouldQuit) {
		// Do the sound stuffs
		for (int i = 0; i < AZA_MAX_STREAMS; i++) {
			azaStreamProcess(&streams[i]);
		}

		azaMutexUnlock(&mutex);
		// TODO: Actually time our runtime so we only sleep if we're not too busy
		azaThreadSleep(1);
		azaMutexLock(&mutex);
	}
	azaMutexUnlock(&mutex);
	return 0;
error:
	return 1;
}



static size_t azaFindDefaultDevice(azaDeviceInfo devicePool[], size_t deviceCount, EDataFlow dataFlow, const char *poolTag) {
#define FAIL_ACTION result = AZA_MAX_DEVICES; goto error
	size_t result = AZA_MAX_DEVICES;
	HRESULT hResult;
	IMMDevice *pDevice;
	IPropertyStore *pPropertyStore;
	char *name;
	hResult = pEnumerator->lpVtbl->GetDefaultAudioEndpoint(pEnumerator, dataFlow, eConsole, &pDevice);
	CHECK_RESULT("GetDefaultAudioEndpoint", FAIL_ACTION);
	hResult = pDevice->lpVtbl->OpenPropertyStore(pDevice, STGM_READ, &pPropertyStore);
	CHECK_RESULT("OpenPropertyStore", FAIL_ACTION);
	PROPERTY_STORE_GET(pPropertyStore, PKEY_Device_FriendlyName, VT_LPWSTR,
		// onSuccess:
		name = wstrToCstr(propVariant.pwszVal),
		// onFail:
		FAIL_ACTION
	);
	// Check by name
	for (size_t i = 0; i < deviceCount; i++) {
		if (strcmp(devicePool[i].name, name) == 0) {
			result = i;
			break;
		}
	}
	if (result == AZA_MAX_DEVICES) {
		AZA_LOG_ERR("Didn't find the default %s device in our list!\n", poolTag);
		FAIL_ACTION;
	} else {
		AZA_LOG_INFO("Default %s device: \"%s\"\n", poolTag, devicePool[result].name);
	}
error:
	SAFE_FREE(name);
	SAFE_RELEASE(pPropertyStore);
	SAFE_RELEASE(pDevice);
	return result;
#undef FAIL_ACTION
}

static void azaWASAPIDeinit() {
	shouldQuit = 1;
	if (azaThreadJoinable(&thread)) {
		azaThreadJoin(&thread);
	}
	azaMutexDeinit(&mutex);
	SAFE_RELEASE(pEnumerator);
	SAFE_RELEASE(pDeviceCollection);
	for (size_t i = 0; i < deviceCount; i++) {
		SAFE_RELEASE(device[i].pDevice);
		SAFE_RELEASE(device[i].pPropertyStore);
		CoTaskMemFree((LPVOID)device[i].idWStr);
		SAFE_FREE((void*)device[i].name);
	}
}

static int azaWASAPIInit() {
	HRESULT hResult;
	hResult = CoInitialize(NULL);
	CHECK_RESULT("CoInitialize", goto error);
	hResult = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, &pEnumerator);
	CHECK_RESULT("CoCreateInstance", goto error);

	hResult = pEnumerator->lpVtbl->EnumAudioEndpoints(pEnumerator, eAll, DEVICE_STATE_ACTIVE, &pDeviceCollection);
	CHECK_RESULT("EnumAudioEndpoints", goto error);
	UINT deviceCountUINT = 0;
	pDeviceCollection->lpVtbl->GetCount(pDeviceCollection, &deviceCountUINT);
	if (deviceCountUINT > AZA_MAX_DEVICES) {
		AZA_LOG_INFO("System has too many devices (%u)! Only enumerating the first %u...\n", deviceCountUINT, AZA_MAX_DEVICES);
		deviceCountUINT = AZA_MAX_DEVICES;
	}
	deviceCount = (size_t)deviceCountUINT;
	for (UINT i = 0; i < deviceCountUINT; i++) {
		IMMDevice *pDevice;
		IMMEndpoint *pEndpoint;
		IPropertyStore *pPropertyStore;
		WCHAR *idWStr;
		char *name;
		// unsigned speakerConfig;
		WAVEFORMATEXTENSIBLE waveFormatEx;
		pDeviceCollection->lpVtbl->Item(pDeviceCollection, i, &pDevice);
		hResult = pDevice->lpVtbl->QueryInterface(pDevice, &IID_IMMEndpoint, &pEndpoint);
		CHECK_RESULT("IMMDevice::QueryInterface(IID_IMMEndpoint)", goto goNext);
		device[i].pDevice = pDevice;
		pDevice->lpVtbl->GetId(pDevice, &idWStr);
		device[i].idWStr = idWStr;

		hResult = pDevice->lpVtbl->OpenPropertyStore(pDevice, STGM_READ, &pPropertyStore);
		CHECK_RESULT("OpenPropertyStore", goto goNext);
		device[i].pPropertyStore = pPropertyStore;
		PROPERTY_STORE_GET(pPropertyStore, PKEY_Device_FriendlyName, VT_LPWSTR,
			// onSuccess:
			name = wstrToCstr(propVariant.pwszVal),
			// onFail:
			goto goNext
		);
		device[i].name = name;
		// PROPERTY_STORE_GET(pPropertyStore, PKEY_AudioEndpoint_PhysicalSpeakers, VT_UI4, speakerConfig = propVariant.uintVal, speakerConfig=0);
		// device[i].speakerConfig = speakerConfig;
		PROPERTY_STORE_GET(pPropertyStore, PKEY_AudioEngine_DeviceFormat, VT_BLOB,
			// onSuccess:
			memcpy(&waveFormatEx, propVariant.blob.pBlobData, AZA_MIN(sizeof(waveFormatEx), propVariant.blob.cbSize)),
			// onFail:
			goto goNext
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
					AZA_LOG_ERR("Unhandled WAVEFORMATEXTENSIBLE::SubFormat " GUID_FORMAT_STR "\n", GUID_ARGS(waveFormatEx.SubFormat));
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
				AZA_LOG_ERR("Unhandled WAVEFORMATEX::wFormatTag %hu\n", waveFormatEx.Format.wFormatTag);
				continue;
		}
		EDataFlow dataFlow;
		hResult = pEndpoint->lpVtbl->GetDataFlow(pEndpoint, &dataFlow);
		CHECK_RESULT("IMMEndpoint::GetDataFlow", goto goNext);
		int render = 0, capture = 0;
		switch (dataFlow) {
			case eRender:
				render = 1;
				break;
			case eCapture:
				capture = 1;
				break;
			case eAll:
				AZA_LOG_INFO("Warning: Device data flow is both capture AND render, which is supposedly impossible.\n");
				render = capture = 1;
				break;
		}
		if (render) {
			deviceOutput[deviceOutputCount++] = device[i];
		}
		if (capture) {
			deviceInput[deviceInputCount++] = device[i];
		}
		if (azaLogLevel >= AZA_LOG_LEVEL_TRACE) {
			char *idCStr = wstrToCstr(idWStr);
			AZA_LOG_TRACE("Device %u:\n\tidStr: \"%s\"\n\tname: \"%s\"\n\tchannels: %u\n\tbitDepth: %u\n\tsamplerate: %zu\n\tspeakerConfig: 0x%x\n", i, idCStr, name, device[i].channels, device[i].sampleBitDepth, device[i].samplerate, device[i].speakerConfig);
			free(idCStr);
		}
goNext:
		SAFE_RELEASE(pPropertyStore);
		SAFE_RELEASE(pEndpoint);
	}
	defaultOutputDevice = azaFindDefaultDevice(deviceOutput, deviceOutputCount, eRender, "output");
	if (defaultOutputDevice == AZA_MAX_DEVICES) goto error;
	defaultInputDevice = azaFindDefaultDevice(deviceInput, deviceInputCount, eCapture, "input");
	if (defaultInputDevice == AZA_MAX_DEVICES) goto error;

	shouldQuit = 0;
	azaMutexInit(&mutex);
	if (azaThreadLaunch(&thread, soundThreadProc, NULL)) {
		AZA_LOG_ERR("Couldn't initialize the sound thread...%d\n", errno);
		goto error;
	}

	return AZA_SUCCESS;
error:
	azaWASAPIDeinit();
	return AZA_ERROR_BACKEND_LOAD_ERROR;
}

static int azaStreamInitWASAPI(azaStream *stream, const char *device) {
	int errCode = AZA_SUCCESS;
	azaDeviceInfo *deviceInfoDefault = NULL;
	azaDeviceInfo *devicePool = NULL;
	azaDeviceInfo *deviceInfo = NULL;
	size_t deviceCount = 0;
	azaStreamData *data = NULL;
	WAVEFORMATEXTENSIBLE *defaultFormat = NULL;
	if (stream->mixCallback == NULL) {
		AZA_LOG_ERR("azaStreamInitWASAPI error: no mix callback provided.\n");
		return AZA_ERROR_NULL_POINTER;
	}
	azaMutexLock(&mutex);
	if (streamCount >= AZA_MAX_STREAMS) {
		AZA_LOG_ERR("azaStreamInitWASAPI error: Too many streams have already been created (%d)!\n", streamCount);
		errCode = AZA_ERROR_BACKEND_ERROR;
		goto error;
	}
	data = azaStreamDataGet();
	switch (stream->deviceInterface) {
		case AZA_OUTPUT:
			deviceInfoDefault = &deviceOutput[defaultOutputDevice];
			devicePool = deviceOutput;
			deviceCount = deviceOutputCount;
			break;
		case AZA_INPUT:
			deviceInfoDefault = &deviceInput[defaultInputDevice];
			devicePool = deviceInput;
			deviceCount = deviceInputCount;
			break;
		default:
			AZA_LOG_ERR("azaStreamInitWASAPI error: stream->deviceInterface (%d) is invalid.\n", stream->deviceInterface);
			errCode = AZA_ERROR_INVALID_CONFIGURATION;
			goto error;
	}
	if (deviceCount == 0) {
		AZA_LOG_ERR("azaStreamInitWASAPI error: There are no %s devices available\n", stream->deviceInterface == AZA_OUTPUT ? "output" : "input");
		errCode = AZA_ERROR_NO_DEVICES_AVAILABLE;
		goto error;
	}
	// Search the nodes for the device name
	for (size_t i = 0; i < deviceCount; i++) {
		azaDeviceInfo *check = &devicePool[i];
		if (strcmp(check->name, device) == 0) {
			deviceInfo = check;
			AZA_LOG_INFO("Chose device by name: \"%s\"\n", device);
			break;
		}
	}
	if (!deviceInfo) {
		deviceInfo = deviceInfoDefault;
		AZA_LOG_INFO("Chose default device: \"%s\"\n", deviceInfo->name);
	}
	data->deviceInfo = deviceInfo;
#define FAIL_ACTION errCode = AZA_ERROR_BACKEND_ERROR; goto error
	HRESULT hResult;
	hResult = deviceInfo->pDevice->lpVtbl->Activate(deviceInfo->pDevice, &IID_IAudioClient, CLSCTX_ALL, NULL, &data->pAudioClient);
	CHECK_RESULT("IMMDevice::Activate(IID_IAudioClient)", FAIL_ACTION);
	// Find out if we natively support our desired format and samplerate, otherwise we have to resample and convert formats
	hResult = data->pAudioClient->lpVtbl->GetMixFormat(data->pAudioClient, (WAVEFORMATEX**)&defaultFormat);
	CHECK_RESULT("IAudioClient::GetMixFormat", FAIL_ACTION);
	WAVEFORMATEXTENSIBLE desiredFormat;
	desiredFormat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	desiredFormat.Format.nChannels = stream->channels ? (WORD)stream->channels : defaultFormat->Format.nChannels;
	desiredFormat.Format.nSamplesPerSec = stream->samplerate ? (DWORD)stream->samplerate : defaultFormat->Format.nSamplesPerSec;
	desiredFormat.Format.wBitsPerSample = 32;
	desiredFormat.Format.nBlockAlign = desiredFormat.Format.nChannels * 4;
	desiredFormat.Format.nAvgBytesPerSec = desiredFormat.Format.nSamplesPerSec * desiredFormat.Format.nBlockAlign;
	desiredFormat.Format.cbSize = sizeof(desiredFormat) - sizeof(desiredFormat.Format);
	desiredFormat.Samples.wValidBitsPerSample = desiredFormat.Format.wBitsPerSample;
	desiredFormat.dwChannelMask = SPEAKER_ALL;
	desiredFormat.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
	WAVEFORMATEXTENSIBLE closestFormat;
	WAVEFORMATEXTENSIBLE *pClosestFormat = &closestFormat;
	hResult = data->pAudioClient->lpVtbl->IsFormatSupported(data->pAudioClient, AUDCLNT_SHAREMODE_SHARED, (WAVEFORMATEX*)&desiredFormat, (WAVEFORMATEX**)&pClosestFormat);
	// TODO: Probably handle the return values of IsFormatSupported (namely AUDCLNT_E_DEVICE_INVALIDATED and AUDCLNT_E_SERVICE_NOT_RUNNING)
	CHECK_RESULT("IAudioClient::IsFormatSupported", FAIL_ACTION);
	int exactFormat = 0;
	if (hResult == S_FALSE) {
		// We got a closest match which is not an exact match
		assert(pClosestFormat != NULL);
		AZA_LOG_INFO("Device \"%s\" doesn't support the exact format desired\n", deviceInfo->name);
		if (closestFormat.Format.nSamplesPerSec != desiredFormat.Format.nSamplesPerSec) {
			AZA_LOG_INFO("\tNo support for %dHz samplerate (closest was %dHz)\n", deviceInfo->name, desiredFormat.Format.nSamplesPerSec, closestFormat.Format.nSamplesPerSec);
		}
		if (closestFormat.Format.nChannels != desiredFormat.Format.nChannels) {
			AZA_LOG_INFO("\tNo support for %hd channels (closest was %hd)\n", deviceInfo->name, desiredFormat.Format.nChannels, closestFormat.Format.nChannels);
		}
		if (closestFormat.Format.wBitsPerSample != desiredFormat.Format.wBitsPerSample) {
			AZA_LOG_INFO("\tNo support for %hd bits per sample (closest was %hd)\n", deviceInfo->name, desiredFormat.Format.wBitsPerSample, closestFormat.Format.wBitsPerSample);
		}
		if (!IsEqualGUID(&closestFormat.SubFormat, &desiredFormat.SubFormat)) {
			AZA_LOG_INFO("\tNo support for IEEE float formats\n");
		}
		data->waveFormatExtensible = closestFormat;
	} else if (hResult == S_OK) {
		// Our exact format is supported
		data->waveFormatExtensible = desiredFormat;
		AZA_LOG_INFO("Device \"%s\" supports the exact format desired :)\n", deviceInfo->name);
		exactFormat = 1;
	} else {
		// Fallback to default
		data->waveFormatExtensible = *defaultFormat;
		AZA_LOG_INFO("Device \"%s\" can't compromise, falling back to default format\n", deviceInfo->name);
	}
	stream->channels = data->waveFormatExtensible.Format.nChannels;
	stream->samplerate = data->waveFormatExtensible.Format.nSamplesPerSec;

	hResult = data->pAudioClient->lpVtbl->Initialize(data->pAudioClient, AUDCLNT_SHAREMODE_SHARED, 0, 10000 * 25, 0, (WAVEFORMATEX*)&data->waveFormatExtensible, NULL);
	CHECK_RESULT("IAudioClient::Initialize", FAIL_ACTION);
	hResult = data->pAudioClient->lpVtbl->GetBufferSize(data->pAudioClient, &data->bufferFrames);
	CHECK_RESULT("IAudioClient::GetBufferSize", FAIL_ACTION);
	AZA_LOG_INFO("Buffer has %u frames\n", data->bufferFrames);
	if (stream->deviceInterface == AZA_OUTPUT) {
		hResult = data->pAudioClient->lpVtbl->GetService(data->pAudioClient, &IID_IAudioRenderClient, &data->pRenderClient);
		CHECK_RESULT("IAudioClient::GetService", FAIL_ACTION);
		uint8_t *buffer;
		hResult = data->pRenderClient->lpVtbl->GetBuffer(data->pRenderClient, data->bufferFrames, &buffer);
		CHECK_RESULT("IAudioRenderClient::GetBuffer", FAIL_ACTION);
		memset(buffer, 0, data->bufferFrames * data->waveFormatExtensible.Format.nBlockAlign);
		hResult = data->pRenderClient->lpVtbl->ReleaseBuffer(data->pRenderClient, data->bufferFrames, 0);
		CHECK_RESULT("IAudioRenderClient::ReleaseBuffer", FAIL_ACTION);
	} else {
		hResult = data->pAudioClient->lpVtbl->GetService(data->pAudioClient, &IID_IAudioCaptureClient, &data->pCaptureClient);
		CHECK_RESULT("IAudioClient::GetService", FAIL_ACTION);
	}

	hResult = data->pAudioClient->lpVtbl->Start(data->pAudioClient);
	CHECK_RESULT("IAudioClient::Start", FAIL_ACTION);

	if (!exactFormat) {
		data->myBuffer = malloc(sizeof(float) * data->bufferFrames * stream->channels);
	} else {
		data->myBuffer = NULL;
	}
	data->stream = stream;
	stream->data = data;
	data->isActive = 1;

	CoTaskMemFree(defaultFormat);
	azaMutexUnlock(&mutex);
	return AZA_SUCCESS;
error:
	CoTaskMemFree(defaultFormat);
	azaStreamDataFree(data);
	SAFE_RELEASE(data->pAudioClient);
	SAFE_RELEASE(data->pRenderClient);
	azaMutexUnlock(&mutex);
	return errCode;
#undef FAIL_ACTION
}

static void azaStreamDeinitWASAPI(azaStream *stream) {
	azaMutexLock(&mutex);
	azaStreamData *data = stream->data;
	HRESULT hResult = data->pAudioClient->lpVtbl->Stop(data->pAudioClient);
	CHECK_RESULT("IAudioClient::Stop", do{}while(0));
	SAFE_RELEASE(data->pAudioClient);
	SAFE_RELEASE(data->pRenderClient);
	azaStreamDataFree(data);
	azaMutexUnlock(&mutex);
}

static size_t azaGetDeviceCountWASAPI(azaDeviceInterface interface) {
	switch (interface) {
		case AZA_OUTPUT: return deviceOutputCount;
		case AZA_INPUT: return deviceInputCount;
		default: return 0;
	}
}

static const char* azaGetDeviceNameWASAPI(azaDeviceInterface interface, size_t index) {
	switch (interface) {
		case AZA_OUTPUT:
			assert(index < deviceOutputCount);
			return deviceOutput[index].name;
			break;
		case AZA_INPUT:
			assert(index < deviceInputCount);
			return deviceInput[index].name;
			break;
		default: return 0;
	}
}

static size_t azaGetDeviceChannelsWASAPI(azaDeviceInterface interface, size_t index) {
	switch (interface) {
		case AZA_OUTPUT:
			assert(index < deviceOutputCount);
			return deviceOutput[index].channels;
			break;
		case AZA_INPUT:
			assert(index < deviceInputCount);
			return deviceInput[index].channels;
			break;
		default: return 0;
	}
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
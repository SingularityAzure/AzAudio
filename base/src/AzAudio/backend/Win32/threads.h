/*
	File: threads.h
	Author: Philip Haynes
	Implementing threads for MSVC because apparently it took 11 years for Microsoft to implement C11 standard features.
*/

#ifndef AZA_THREADS_WIN32_H
#define AZA_THREADS_WIN32_H

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <synchapi.h>
#include <handleapi.h>
#include <sysinfoapi.h>
#include <processthreadsapi.h>
#include <process.h>

#include <assert.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#define AZA_MSVC_ONLY(a) a
#include <timeapi.h>
#else
#define AZA_MSVC_ONLY(a)
#endif

typedef struct azaThread {
	HANDLE hThread;
	unsigned id;
} azaThread;

// returns 0 on success, errno on failure
static inline int azaThreadLaunch(azaThread *thread, unsigned (__stdcall *proc)(void*), void *userdata) {
	thread->hThread = (HANDLE)_beginthreadex(NULL, 0, proc, userdata, 0, &thread->id);
	if (thread->hThread == NULL) {
		return errno;
	}
	return 0;
}

static inline int azaThreadJoinable(azaThread *thread) {
	return thread->hThread != NULL;
}

static inline void azaThreadJoin(azaThread *thread) {
	assert(thread->id != GetCurrentThreadId());
	assert(thread->hThread != NULL);
	WaitForSingleObject(thread->hThread, 0xfffffff1);
	CloseHandle(thread->hThread);
	thread->hThread = NULL;
	thread->id = 0;
}

static inline void azaThreadDetach(azaThread *thread) {
	assert(azaThreadJoinable(thread));
	CloseHandle(thread->hThread);
	thread->hThread = NULL;
	thread->id = 0;
}

static inline void azaThreadSleep(uint32_t milliseconds) {
	AZA_MSVC_ONLY(timeBeginPeriod(1));
	Sleep(milliseconds);
	AZA_MSVC_ONLY(timeEndPeriod(1));
}

static inline void azaThreadYield() {
	Sleep(0);
}

typedef struct azaMutex {
	CRITICAL_SECTION criticalSection;
} azaMutex;

static inline void azaMutexInit(azaMutex *mutex) {
	InitializeCriticalSection(&mutex->criticalSection);
}

static inline void azaMutexDeinit(azaMutex *mutex) {
	DeleteCriticalSection(&mutex->criticalSection);
}

static inline void azaMutexLock(azaMutex *mutex) {
	EnterCriticalSection(&mutex->criticalSection);
}

static inline void azaMutexUnlock(azaMutex *mutex) {
	LeaveCriticalSection(&mutex->criticalSection);
}

#ifdef __cplusplus
}
#endif

#endif // AZA_THREADS_WIN32_H

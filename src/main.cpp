/*
    File: main.cpp
    Author: singularity
    Simple test program for our library
*/

#include <iostream>

#include "log.hpp"
#include "audio.h"

#ifdef __unix
#include <csignal>
#include <cstdlib>
#include <execinfo.h>
#include <unistd.h>

void handler(int sig) {
    void *array[50];
    size_t size = backtrace(array, 50);
    char **strings;
    strings = backtrace_symbols(array, size);
    sys::cout <<  "Error: signal " << sig << std::endl;
    for (uint32_t i = 0; i < size; i++) {
        sys::cout << strings[i] << std::endl;
    }
    free(strings);
    exit(1);
}
#endif

void logCallback(const char* message) {
    sys::cout << "AzureAudio: " << message << std::endl;
}

int main(int argumentCount, char** argumentValues) {
    #ifdef __unix
    signal(SIGSEGV, handler);
    #endif
    try {
        azaInit();
        azaSetLogCallback(logCallback);
        azaStream stream;
        if (azaMicTestStart(&stream) != AZA_SUCCESS) {
            throw std::runtime_error("Failed to start mic test!");
        }
        sys::cout << "Press ENTER to stop" << std::endl;
        std::cin.get();
        if (azaMicTestStop(&stream) != AZA_SUCCESS) {
            throw std::runtime_error("Error while trying to stop mic test!");
        }
    } catch (std::runtime_error& e) {
        sys::cout << "Runtime Error: " << e.what() << std::endl;
    }
    return 0;
}

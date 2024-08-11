cmake_minimum_required(VERSION 3.25)

set(CMAKE_DEBUG_POSTFIX _debug)

project(AzAudio)

include(GNUInstallDirs)

if (NOT ${CMAKE_INSTALL_PREFIX})
	set(CMAKE_INSTALL_PREFIX $ENV{CMAKE_INSTALL_PREFIX})
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (MSVC)
	add_compile_options(/W3)
else()
	add_compile_options(-Wall -fmax-errors=1)
	if (CMAKE_SYSTEM MATCHES Windows)
		add_compile_options(-D_GLIBCXX_USE_NANOSLEEP)
		add_link_options(-static-libgcc -static-libstdc++ -static -Llibwinpthread-1.dll)
	endif()
endif()

add_subdirectory(base)

add_subdirectory(tests)
if (CMAKE_SYSTEM MATCHES Windows)
	set(TARGET_PLATFORM_NAME Win32)
elseif (CMAKE_SYSTEM MATCHES Linux)
	set(TARGET_PLATFORM_NAME Linux)
endif()

file(GLOB BACKEND_SOURCES "src/AzAudio/backend/${TARGET_PLATFORM_NAME}/*")

add_library(AzAudio STATIC
	src/AzAudio/AzAudio.h
	src/AzAudio/AzAudio.c
	src/AzAudio/dsp.h
	src/AzAudio/dsp.c
	src/AzAudio/error.h
	src/AzAudio/helpers.h
	src/AzAudio/helpers.c
	# backend
	src/AzAudio/backend/backend.h
	src/AzAudio/backend/interface.h
	src/AzAudio/backend/interface.c
	${BACKEND_SOURCES}
)

set_target_properties(AzAudio PROPERTIES
	DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)

target_include_directories(AzAudio PUBLIC
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/base/src>
	$<INSTALL_INTERFACE:include>
)

if (CMAKE_SYSTEM MATCHES Windows)
	target_link_libraries(AzAudio INTERFACE ksuser)
elseif (CMAKE_SYSTEM MATCHES Linux)
	target_link_libraries(AzAudio INTERFACE pthread)

	find_package(PkgConfig REQUIRED)
	pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
	target_include_directories(AzAudio PRIVATE ${PIPEWIRE_INCLUDE_DIRS})
	target_compile_options(AzAudio PRIVATE ${PIPEWIRE_CFLAGS_OTHER})
endif()

# installation

install(TARGETS AzAudio
	EXPORT AzAudio
	CONFIGURATIONS Debug Release
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/AzAudio
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/AzAudio
)
install(EXPORT AzAudio
	FILE AzAudio.cmake
	NAMESPACE AzAudio::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AzAudio
)
install(FILES AzAudioConfig.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AzAudio
)
install(DIRECTORY src/AzAudio
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	FILES_MATCHING PATTERN "*.h"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES SOURCE_LIST)
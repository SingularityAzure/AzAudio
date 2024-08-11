add_executable(mic_test
	src/log.hpp
	src/log.cpp
	src/main.cpp
)

target_include_directories(mic_test PUBLIC ${PROJECT_SOURCE_DIR}/base/src)

target_link_libraries(mic_test PRIVATE AzAudio)

set_target_properties(mic_test PROPERTIES
	DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

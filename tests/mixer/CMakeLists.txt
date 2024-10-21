add_executable(mixer
	src/main.c
)

target_include_directories(mixer PUBLIC ${PROJECT_SOURCE_DIR}/base/src)
target_include_directories(mixer PUBLIC ${PROJECT_SOURCE_DIR}/external/stb)

target_link_libraries(mixer PRIVATE AzAudio)

set_target_properties(mixer PROPERTIES
	DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

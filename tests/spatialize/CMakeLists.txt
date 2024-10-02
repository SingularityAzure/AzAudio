add_executable(spatialize
	src/main.c
)

target_include_directories(spatialize PUBLIC ${PROJECT_SOURCE_DIR}/base/src)
target_include_directories(spatialize PUBLIC ${PROJECT_SOURCE_DIR}/external/stb)

target_link_libraries(spatialize PRIVATE AzAudio)

set_target_properties(spatialize PROPERTIES
	DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

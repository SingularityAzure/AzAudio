@echo off

setlocal enabledelayedexpansion

set BuildDebug=0
set BuildRelease=0
set has_args=0
set run_arg=0
set run=0
set run_debug=0
set run_target=
set run_args=
set clean=0
set install=0
set trace=
set verbose=

set /a argCount=0
for %%a in (%*) do (
	set "argValues[!argCount!]=%%~a"
	set /a argCount+=1
)

set /a argIndex=0
:loopStart
set arg=!argValues[%argIndex%]!
if %run_arg% == 1 (
	set run_target=%arg%
	set /a run_arg=2
	goto Done
)
if %run_arg% == 2 (
	if .!run_args!==. (
		set run_args="%arg%"
	) else (
		set run_args="!run_args! %arg%"
	)
	echo !run_args!
	goto Done
)
set arg=Arg%arg%
goto %arg%
goto Arg
:ArgAll
	set BuildDebug=1
	set BuildRelease=1
	goto Done
:ArgRelease
	set BuildRelease=1
	goto Done
:ArgDebug
	set BuildDebug=1
	goto Done
:Argrun
	set run_arg=1
	set run=1
	set run_debug=0
	goto Done
:Argrun_debug
	set run_arg=1
	set run=0
	set run_debug=1
	goto Done
:Argclean
	set clean=1
	goto Done
:Argtrace
	set trace=--trace
	goto Done
:Argverbose
	set verbose=--verbose
	goto Done
:Arginstall
	set install=1
	goto Done
:Arg
:Arg%arg%
	echo "Usage: build.bat [clean]? [verbose]? [trace]? [install]? [All|Debug|Release]? ([run|run_debug] project_name)?"
	goto EndOfScript
:Done
set /a argIndex+=1
if %argIndex% neq %argCount% goto loopStart


if %clean% == 1 (
	for /f %%i in ('dir /a:d /b "tests\*"') do rd /s /q tests\%%i\bin
	rd /s /q build
)

if %BuildDebug% == 1 (
	echo "Building Win32 Debug"
	md build
	cd build
	cmake %trace% ..
	if ERRORLEVEL 1 (
		echo "CMake configure failed! Aborting..."
		goto EndOfScript
	)
	cmake --build . %verbose% -j %number_of_processors% --config Debug
	if ERRORLEVEL 1 (
		echo "CMake build failed! Aborting..."
		goto EndOfScript
	)
	if %install% == 1 (
		cmake --install . %verbose% --config Debug
	)
	cd ..
)

if %BuildRelease% == 1 (
	echo "Building Win32 Release"
	md build
	cd build
	cmake %trace% ..
	if ERRORLEVEL 1 (
		echo "CMake configure failed! Aborting..."
		goto EndOfScript
	)
	cmake --build . %verbose% -j %number_of_processors% --config Release
	if ERRORLEVEL 1 (
		echo "CMake build failed! Aborting..."
		goto EndOfScript
	)
	if %install% == 1 (
		cmake --install . %verbose% --config Release
	)
	cd ..
)

if %run% == 1 (
	cd tests\%run_target%
	bin\Release\%run_target%.exe %run_args%
	cd ..\..
)

if %run_debug% == 1 (
	cd tests\%run_target%
	bin\Debug\%run_target%_debug.exe %run_args%
	@REM %~dp0tests\%run_target%\bin\Debug\%run_target%_debug.exe %run_args%
	cd ..\..
)
if %errorlevel% NEQ 0 (
	echo "Failed with code " %errorlevel%
	goto EndOfScript
)

echo "All builds complete!"

:EndOfScript

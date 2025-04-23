@echo off
setlocal

:: --- Configuration ---
set BUILD_DIR=build
set CMAKE_GENERATOR="Visual Studio 17 2022"
set CMAKE_PLATFORM="x64"

:: --- Default values ---
set BUILD_CONFIG=Debug
set CLEAN_BUILD=0
set DO_INSTALL=0

:: --- Argument Parsing ---
:PARSE_ARGS
if "%~1"=="" goto :PARSE_ARGS_DONE

:: Re-check for help arguments in case they are not the first one (optional, but robust)
if /i "%~1"=="--help" goto :SHOW_USAGE

if /i "%~1"=="--clean" (
    set CLEAN_BUILD=1
    :: move up to next argument
    shift /1
    goto :PARSE_ARGS
)

if /i "%~1"=="--install" (  
    set DO_INSTALL=1
    :: move up to next argument
    shift /1
    goto :PARSE_ARGS
)

:: Assume the next argument (if present) is the build configuration
if not "%~1"=="" (
    set BUILD_CONFIG=%1
    :: move up to next argument
    shift /1
    echo last arg: "%1"
)

:PARSE_ARGS_DONE

:: --- Change to script's directory ---
:: This changes the working directory for the entire script execution.
cd /d %~dp0 || (
    echo ERROR: Cannot change to script directory '%~dp0'.
    pause
    exit /b 1
)

echo ================================================
echo Working Directory:   %CD%
echo Build Directory:     %BUILD_DIR%
echo Build Configuration: %BUILD_CONFIG%
echo Generator:           %CMAKE_GENERATOR%
echo Platform:            %CMAKE_PLATFORM%
echo Clean Build:         %CLEAN_BUILD%
echo Install after Build: %DO_INSTALL%
echo ================================================
echo.

:: --- Clean Step (Optional) ---
if %CLEAN_BUILD% equ 1 (
    if exist "%BUILD_DIR%" (
        echo Removing existing build directory...
        rmdir /s /q "%BUILD_DIR%" || ( echo ERROR: Failed to remove directory '%BUILD_DIR%'. & goto :ERROR_EXIT_NOPOPD )
    ) else (
        echo Build directory '%BUILD_DIR%' does not exist. Nothing to clean.
    )
)

:: --- Prepare Build Directory ---
:: Create build directory if it doesn't exist
if not exist "%BUILD_DIR%" (
    echo Creating build directory...
    mkdir "%BUILD_DIR%" || ( echo ERROR: Failed to create directory '%BUILD_DIR%'. & goto :ERROR_EXIT_NOPOPD )
)
:: Use pushd to easily return with popd later
pushd "%BUILD_DIR%" || ( echo ERROR: Cannot change to build directory '%BUILD_DIR%'. & goto :ERROR_EXIT_NOPOPD )

:: --- Configure Step ---
echo Configuring CMake...
:: Note: -S .. means source is the parent directory relative to the current (build) directory.
cmake -S .. -B . -G %CMAKE_GENERATOR% -A %CMAKE_PLATFORM%
if %errorlevel% neq 0 ( echo ERROR: CMake configuration failed. & goto :ERROR_EXIT )

:: --- Build Step ---
echo Building project ^(%BUILD_CONFIG%^)...
cmake --build . --config %BUILD_CONFIG%
if %errorlevel% neq 0 ( echo ERROR: CMake build failed for configuration '%BUILD_CONFIG%'. & goto :ERROR_EXIT )

:: --- Install Step (Optional) ---
if %DO_INSTALL% equ 1 (
    echo Installing project ^(%BUILD_CONFIG%^)...
    cmake --install . --config %BUILD_CONFIG%
    if %errorlevel% neq 0 ( echo ERROR: CMake install failed for configuration '%BUILD_CONFIG%'. & goto :ERROR_EXIT )
)

goto :SUCCESS_EXIT

:: --- Error Handling ---
:ERROR_EXIT
:: Pop build directory because we were inside it when error occurred
popd
:ERROR_EXIT_NOPOPD
echo.
echo *** BUILD FAILED ***
echo.
:: setlocal ensures variables don't leak, endlocal is implicit on exit
exit /b 1

:: --- Success ---
:SUCCESS_EXIT
:: Pop build directory, returning to script's directory
popd
echo.
if %DO_INSTALL% equ 1 (
    echo *** BUILD AND INSTALL SUCCEEDED ^(%BUILD_CONFIG%^) ***
) else (
    echo *** BUILD SUCCEEDED ^(%BUILD_CONFIG%^) ***
)
echo.
:: setlocal ensures variables don't leak, endlocal is implicit on exit
exit /b 0

:: --- Usage Message ---
:SHOW_USAGE
echo.
echo Usage: %~nx0 [Options] [Configuration]
echo.
echo Options:
echo   --help         Displays this help message.
echo   --clean        Removes the '%BUILD_DIR%' directory before configuration.
echo   --install      Runs the install step after a successful build.
echo.
echo Configuration:
echo   Specifies the build configuration (e.g., Debug, Release, RelWithDebInfo).
echo   If omitted, defaults to '%BUILD_CONFIG%'.
echo.
echo Examples:
echo   %~nx0                           (Builds %BUILD_CONFIG% configuration)
echo   %~nx0 Release                   (Builds Release configuration)
echo   %~nx0 --clean                   (Cleans, then builds %BUILD_CONFIG% configuration)
echo   %~nx0 --clean Release           (Cleans, then builds Release configuration)
echo   %~nx0 --install                 (Builds %BUILD_CONFIG% configuration and installs)
echo   %~nx0 --install Release         (Builds Release configuration and installs)
echo   %~nx0 --clean --install Release (Cleans, builds Release configuration, and installs)
echo.
echo Default Generator: %CMAKE_GENERATOR%
echo Default Platform:  %CMAKE_PLATFORM%
echo.
:: setlocal ensures variables don't leak, endlocal is implicit on exit
exit /b 0
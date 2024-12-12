@echo off & cd /d %~dp0

if "%~1" EQU "" (
    echo Usage: %0 ^<lib_source_dir^> ^<lib_binary_dir^> ^<lib_resources_dir^> ^<install_dir^>
    exit /b 1
)

setlocal
set "lib_source_dir=%~1"
set "lib_binary_dir=%~2"
set "lib_resources_dir=%~3"
set "install_dir=%~4"

:: Trim trailing path slash (if exists)
if "%lib_source_dir:~-1%" == "\" set "lib_source_dir=%lib_source_dir:~0,-1%"
if "%lib_binary_dir:~-1%" == "\" set "lib_binary_dir=%lib_binary_dir:~0,-1%"
if "%lib_resources_dir:~-1%" == "\" set "lib_resources_dir=%lib_resources_dir:~0,-1%"
if "%install_dir:~-1%" == "\" set "install_dir=%install_dir:~0,-1%"

echo Installation start..
echo lib_source_dir    : "%lib_source_dir%"
echo lib_binary_dir    : "%lib_binary_dir%"
echo lib_resources_dir : "%lib_resources_dir%"
echo install_dir       : "%install_dir%"

if not exist "%lib_source_dir%\" (
    echo Error: Lib source directory not found.
    goto :loc_error
)

if not exist "%lib_binary_dir%\" (
    echo Error: Lib build directory not found.
    goto :loc_error
)

if not exist "%lib_resources_dir%\" (
    echo Error: Lib resources directory not found.
    goto :loc_error
)

if not exist "%install_dir%\" (
    echo Creating install directory..
    mkdir "%install_dir%"
)

echo Installing lib headers..
robocopy "%lib_source_dir%" "%install_dir%\include\triengine" /IF *.h *.hh *.hpp *.hxx /MIR /R:1 /W:1 /NFL /NDL /NJH /NJS /NP /NS /NC >NUL
if %ERRORLEVEL% GEQ 8 (
    echo Failed to install lib headers. ^(Error code: %ERRORLEVEL%^)
    goto :loc_error
)

echo Installing lib binaries..
robocopy "%lib_binary_dir%" "%install_dir%\lib" /MIR /R:1 /W:1 /NFL /NDL /NJH /NJS /NP /NS /NC >NUL
if %ERRORLEVEL% GEQ 8 (
    echo Failed to install lib binaries. ^(Error code: %ERRORLEVEL%^)
    goto :loc_error
)

echo Installing lib resources..
robocopy "%lib_resources_dir%" "%install_dir%\resources" /MIR /R:1 /W:1 /NFL /NDL /NJH /NJS /NP /NS /NC >NUL
if %ERRORLEVEL% GEQ 8 (
    echo Failed to install lib resources. ^(Error code: %ERRORLEVEL%^)
    goto :loc_error
)

:loc_success
echo Installation success!
endlocal & exit /b 0

:loc_error
echo Installation failed!!
endlocal & exit /b 1

::
:: Ref: https://docs.microsoft.com/en-us/windows-server/administration/windows-commands/robocopy
::
:: Robocopy command syntax:
::     `robocopy <Source> <Destination> [<File>[ ...]] [<Options>]`
:: 
:: Robocopy exit(return) codes:
::     0   OK        
::     1   OKCOPY    
::     2   EXTRA     
::     4   MISMATCH  
::     8   FAIL      
::     16  ERROR     
::     (NOTE: `>=8` exit code indicates that there was at least one failure during the copy operation.)
::
:: Robocopy options explained:
:: /MIR  - Mirror mode: Exactly replicate source directory structure
::         - Deletes files/directories in destination not present in source
::         - Ensures destination is an exact copy of source
::
:: /R:1  - Retry failed copies once
::         - Helps handle temporary file access issues
::
:: /W:1  - Wait 1 second between retries
::         - Prevents immediate retry flooding
::
:: /NFL  - No File List: Don't log file names
::         - Keeps log cleaner, focuses on summary
::
:: /NDL  - No Directory List: Don't log directory names
::         - Further reduces log noise
:: /NJH  - No Job Header.
:: /NJS  - No Job Summary./NS  : No Size - don't log file sizes.
:: /NC   - No Class - don't log file classes.
:: /NP   - No Progress: Don't display percentage copied
::         - Reduces console clutter

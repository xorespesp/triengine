@cd /d %~dp0 & echo off
set "bin_file=..\bin\x64-Release\example-bvh\example-bvh.exe"

"%bin_file%" %* || (goto :loc_error_exit)

:loc_exit
echo.
echo ================================================================================
echo Program terminated!
choice /C qp /D q /T 5 /N /M "Press [P] to pause, [Q] to quit. [Q] will be selected after 5 secs:"
if %errorlevel% equ 2 (
  echo Press any key to quit.. & pause > nul
)
exit /b

:loc_error_exit
echo.
echo ================================================================================
echo Program terminated unsuccessfully! (returns: %errorlevel%)
echo Press any key to quit..
pause > nul & exit /b
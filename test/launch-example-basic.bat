@cd /d %~dp0 && powershell.exe ^
	-NoProfile ^
	-ExecutionPolicy Bypass ^
	-File ".\launch.ps1" ^
	"x64-Release\example-basic\example-basic.exe" %*
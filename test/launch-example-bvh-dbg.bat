@cd /d %~dp0 && powershell.exe ^
	-NoProfile ^
	-ExecutionPolicy Bypass ^
	-File ".\launch.ps1" ^
	"x64-Debug\example-bvh\example-bvh.exe" %*
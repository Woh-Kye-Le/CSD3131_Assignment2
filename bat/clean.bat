@echo off
setlocal

for %%I in ("%~dp0..") do set "ROOT=%%~fI"

echo =========================================
echo  SeamCarving - Clean Local Build Folders
echo =========================================
echo Root: %ROOT%
echo.

if exist "%ROOT%\.vs" (
    echo Removing .vs...
    rmdir /s /q "%ROOT%\.vs"
)

if exist "%ROOT%\x64" (
    echo Removing x64...
    rmdir /s /q "%ROOT%\x64"
)

if exist "%ROOT%\build" (
    echo Removing build...
    rmdir /s /q "%ROOT%\build"
)

if exist "%ROOT%\SeamCarving\x64" (
    echo Removing SeamCarving\x64...
    rmdir /s /q "%ROOT%\SeamCarving\x64"
)

if exist "%ROOT%\SeamCarving\SeamCarving.vcxproj.user" (
    echo Removing SeamCarving.vcxproj.user...
    del /q "%ROOT%\SeamCarving\SeamCarving.vcxproj.user"
)

if exist "%ROOT%\SeamCarving\output" (
    echo Clearing SeamCarving\output...
    del /q "%ROOT%\SeamCarving\output\*" >nul 2>&1
    type nul > "%ROOT%\SeamCarving\output\.gitkeep"
)

echo.
echo Clean finished.
echo.
if /I not "%NO_PAUSE%"=="1" pause
endlocal

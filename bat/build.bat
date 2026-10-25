@echo off
setlocal

for %%I in ("%~dp0..") do set "ROOT=%%~fI"

echo =========================================
echo  SeamCarving - Build (Release)
echo =========================================
echo Root: %ROOT%
echo.

echo [1/2] Configuring CMake...
cmake -S "%ROOT%" -B "%ROOT%\build" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto :failed

echo [2/2] Building Release...
cmake --build "%ROOT%\build" --config Release
if errorlevel 1 goto :failed

echo.
echo Build finished successfully.
goto :done

:failed
echo.
echo Build failed.

:done
echo.
if /I not "%NO_PAUSE%"=="1" pause
endlocal

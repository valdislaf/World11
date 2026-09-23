@echo off
setlocal ENABLEDELAYEDEXPANSION

set "MODE=%~1"
if "%MODE%"=="" goto :all

if /I "%MODE%"=="mingw" goto :mingw
if /I "%MODE%"=="vs"    goto :vs
if /I "%MODE%"=="clean" goto :clean

echo [ERROR] Unknown mode "%MODE%"
goto :help

:all

call :mingw
if errorlevel 1 exit /b %ERRORLEVEL%

call :vs
exit /b %ERRORLEVEL%


:mingw
set "MSYS64=C:\msys64"
set "TOOLCHAIN_ROOT=%MSYS64%\mingw64"
set "MINGW_BIN=%TOOLCHAIN_ROOT%\bin"
set "BUILD_DIR=cmake-build-debug-msys2mingw"

if not exist "%MINGW_BIN%\g++.exe" (
  echo [ERROR] g++.exe not found in "%MINGW_BIN%"
  exit /b 1
)

set "PATH=%MINGW_BIN%;%PATH%"

cmake -S . -B "%BUILD_DIR%" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_CXX_COMPILER="%MINGW_BIN%\g++.exe" ^
  -Dglfw3_DIR="%TOOLCHAIN_ROOT%\lib\cmake\glfw3"
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --config Debug
exit /b %ERRORLEVEL%


:vs
if not defined VCPKG_ROOT set "VCPKG_ROOT=C:\vcpkg"
set "VS_BUILD_DIR=vsbuild"
set "GLFW3_VS_DIR=%VCPKG_ROOT%\installed\x64-windows\share\glfw3"

if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
  echo [ERROR] vcpkg toolchain not found: "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
  exit /b 1
)
if not exist "%GLFW3_VS_DIR%\glfw3Config.cmake" (
  echo [ERROR] vcpkg GLFW config not found: "%GLFW3_VS_DIR%\glfw3Config.cmake"
  echo         Install package first, for example: vcpkg install glfw3:x64-windows
  exit /b 1
)

rmdir /S /Q "%VS_BUILD_DIR%" 2>nul

cmake -S . -B "%VS_BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE:FILEPATH="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
  -Dglfw3_DIR:PATH="%GLFW3_VS_DIR%"
if errorlevel 1 exit /b 1

cmake --build "%VS_BUILD_DIR%" --config Debug
if errorlevel 1 exit /b 1

echo [OK] Visual Studio build generated in "vsbuild"
exit /b %ERRORLEVEL%


:clean
rmdir /S /Q cmake-build-debug-msys2mingw 2>nul
rmdir /S /Q vsbuild 2>nul
del /Q horizongates.exe 2>nul
exit /b 0


:help
echo Usage: build_win.bat ^<mingw^|vs^|clean^>
exit /b 1

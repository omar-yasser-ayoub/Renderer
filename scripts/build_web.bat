@echo off
set ROOT=%~dp0..
set BUILD_DIR=%ROOT%\build\web
set EMSDK=%ROOT%\third_party\emsdk

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem The web build is a separate target (new_renderer_web) built with the
rem Emscripten toolchain: WebGPU runs in the browser through Dawn's
rem emdawnwebgpu port. Serve the build directory over http afterwards:
rem   npx http-server "%BUILD_DIR%" -p 8080
call "%EMSDK%\upstream\emscripten\emcmake.bat" cmake -S "%ROOT%" -B "%BUILD_DIR%" -G Ninja -DRHI_BACKEND=WEBGPU -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 ( echo Configure failed. & exit /b 1 )

cmake --build "%BUILD_DIR%" --parallel
if %ERRORLEVEL% neq 0 ( echo Build failed. & exit /b 1 )

echo Built web target: "%BUILD_DIR%\new_renderer_web.html"

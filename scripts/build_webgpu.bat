@echo off
set ROOT=%~dp0..
set BUILD_DIR=%ROOT%\build\webgpu

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

cmake -S "%ROOT%" -B "%BUILD_DIR%" -G Ninja -DRHI_BACKEND=WEBGPU -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
if %ERRORLEVEL% neq 0 ( echo Configure failed. & exit /b 1 )

cmake --build "%BUILD_DIR%" --parallel
if %ERRORLEVEL% neq 0 ( echo Build failed. & exit /b 1 )

copy /Y "C:\Dawn\SDK\bin\d3dcompiler_47.dll" "%BUILD_DIR%\"
copy /Y "C:\Dawn\SDK\bin\vulkan-1.dll" "%BUILD_DIR%\"

echo Built WebGPU backend.

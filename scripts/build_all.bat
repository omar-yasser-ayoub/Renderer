@echo off
rem Builds all three backends, each into its own build dir.
set SCRIPTS=%~dp0

call "%SCRIPTS%build_vk.bat"
if %ERRORLEVEL% neq 0 ( echo Vulkan build failed. & exit /b 1 )

call "%SCRIPTS%build_dx12.bat"
if %ERRORLEVEL% neq 0 ( echo DX12 build failed. & exit /b 1 )

call "%SCRIPTS%build_webgpu.bat"
if %ERRORLEVEL% neq 0 ( echo WebGPU build failed. & exit /b 1 )

echo Built all backends.

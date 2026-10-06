@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
set "PATH=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"

echo === Running CMake Configure with Ninja ===
cmake -B build_win -G Ninja -DCMAKE_BUILD_TYPE=Release -DCOPY_PLUGIN_AFTER_BUILD=FALSE
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configure failed.
    exit /b %ERRORLEVEL%
)

echo === Building AutoLeveler_VST3 ===
cmake --build build_win --config Release --target AutoLeveler_VST3
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed.
    exit /b %ERRORLEVEL%
)

echo === Deploying VST3 to System Directory ===
if exist "build_win\AutoLeveler_artefacts\Release\VST3\Atafuta09Leveler.vst3" (
    xcopy /E /I /Y /Q "build_win\AutoLeveler_artefacts\Release\VST3\Atafuta09Leveler.vst3" "C:\Program Files\Common Files\VST3\Atafuta09Leveler.vst3"
    echo [SUCCESS] Deployed to "C:\Program Files\Common Files\VST3\Atafuta09Leveler.vst3"
) else (
    echo [WARNING] VST3 bundle not found in build_win artefacts.
)

echo === Complete! ===

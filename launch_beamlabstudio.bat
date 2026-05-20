@echo off
set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%build
if not exist "%BUILD_DIR%\beamlab_ui.exe" (
    echo Error: beamlab_ui.exe not found in %BUILD_DIR%. Build first.
    exit /b 1
)
cd /d "%BUILD_DIR%"
beamlab_ui.exe %*

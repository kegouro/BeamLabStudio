@echo off
setlocal enabledelayedexpansion
set ARGS=
:loop
if "%~1"=="" goto run
set ARGS=!ARGS! "%~1"
shift
goto loop
:run
python "%~dp0run_beamlab_full.py" !ARGS!
endlocal

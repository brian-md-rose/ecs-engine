@echo off

setlocal EnableExtensions
cd /d "%~dp0.."

echo Generating Visual Studio solution and project files
cmake -S . -B build/editor -DENGINE_BUILD_EDITOR=ON
pause
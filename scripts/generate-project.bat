@echo off

setlocal EnableExtensions
cd /d "%~dp0.."

echo Generating Visual Studio solution and project files
cmake --preset msvc
pause
@echo off
rem Tags the current main commit as a release and pushes the tag, which starts the
rem Release workflow. Run it after the release PR is merged (see docs/releasing.md).
rem
rem Usage: scripts/release.bat 0.4.0   (a leading "v" is accepted)

setlocal EnableExtensions
cd /d "%~dp0.."

if "%~1"=="" goto usage
set "VERSION=%~1"
if /i "%VERSION:~0,1%"=="v" set "VERSION=%VERSION:~1%"
set "TAG=v%VERSION%"

echo %VERSION%| findstr /r /x "[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*" >nul
if errorlevel 1 (
    echo error: "%~1" is not a version of the form x.y.z
    exit /b 1
)

rem Must be on a clean main that matches origin/main.
set "BRANCH="
for /f "delims=" %%b in ('git rev-parse --abbrev-ref HEAD') do set "BRANCH=%%b"
if not "%BRANCH%"=="main" (
    echo error: on branch "%BRANCH%"; switch to main first
    exit /b 1
)

set "DIRTY="
for /f "delims=" %%s in ('git status --porcelain') do set "DIRTY=1"
if defined DIRTY (
    echo error: the working tree has uncommitted changes
    exit /b 1
)

echo Fetching origin...
git fetch --quiet origin main
if errorlevel 1 (
    echo error: git fetch failed
    exit /b 1
)
for /f "delims=" %%h in ('git rev-parse HEAD') do set "HEAD_SHA=%%h"
for /f "delims=" %%h in ('git rev-parse origin/main') do set "ORIGIN_SHA=%%h"
if not "%HEAD_SHA%"=="%ORIGIN_SHA%" (
    echo error: local main is not the same commit as origin/main; pull or push first
    exit /b 1
)

rem The Release workflow rejects a tag that doesn't match CMakeLists.txt, so check first.
findstr /l /c:"    VERSION %VERSION%" CMakeLists.txt >nul
if errorlevel 1 (
    echo error: CMakeLists.txt does not say VERSION %VERSION%; prepare the release first
    exit /b 1
)
findstr /l /c:"## [%VERSION%] - " CHANGELOG.md >nul
if errorlevel 1 (
    echo error: CHANGELOG.md has no "## [%VERSION%]" section
    exit /b 1
)

git rev-parse -q --verify "refs/tags/%TAG%" >nul
if not errorlevel 1 (
    echo error: tag %TAG% already exists locally
    exit /b 1
)
git ls-remote --exit-code --tags origin "refs/tags/%TAG%" >nul
if not errorlevel 1 (
    echo error: tag %TAG% already exists on origin
    exit /b 1
)

echo.
git log --oneline -1
set "ANSWER="
set /p "ANSWER=Tag this commit as %TAG% and push it to origin? [y/N] "
if /i not "%ANSWER%"=="y" (
    echo Cancelled.
    exit /b 1
)

git tag -a "%TAG%" -m "%TAG%"
if errorlevel 1 (
    echo error: git tag failed
    exit /b 1
)
git push origin "%TAG%"
if errorlevel 1 (
    echo error: push failed; the local tag %TAG% was kept. Retry with: git push origin %TAG%
    exit /b 1
)

echo.
echo Pushed %TAG%. The Release workflow will now create the GitHub release.
exit /b 0

:usage
echo Usage: %~nx0 ^<version^>     e.g. %~nx0 0.4.0
exit /b 1

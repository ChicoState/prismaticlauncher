@echo off
setlocal
rem Build and run the parent checkout using its Ubuntu WSL toolchain.
wsl.exe -d Ubuntu --cd "%~dp0.." -- bash ./scripts/build-and-run.sh %*
set "build_result=%errorlevel%"
if not "%build_result%"=="0" (
    echo.
    echo Build, tests, or application failed with exit code %build_result%.
    pause
)
exit /b %build_result%

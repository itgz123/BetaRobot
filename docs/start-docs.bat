@echo off
cd /d "%~dp0"
if errorlevel 1 (
    echo Failed to change to docs directory
    pause
    exit /b 1
)
echo.
echo Starting BetaRobot docs...
echo.
call npm run docs:dev
if errorlevel 1 (
    echo.
    echo Failed to start. Make sure Node.js is installed.
)
pause
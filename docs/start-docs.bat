@echo off
cd /d "%~dp0" || (
    echo ERROR: Cannot find docs folder
    pause
    exit /b 1
)

echo Checking npm...
where npm >nul 2>&1
if errorlevel 1 (
    echo ERROR: npm not found. Install Node.js first.
    pause
    exit /b 1
)

if not exist "node_modules" (
    echo ^(First run - installing dependencies...^)
    call npm install
    if errorlevel 1 (
        echo.
        echo npm install failed
        pause
        exit /b 1
    )
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
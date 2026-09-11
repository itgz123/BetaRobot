@echo off
REM
REM check-docs.bat - 文档站点本地构建校验
REM
REM 1. 检查 npm
REM 2. 缺少 node_modules 时安装依赖
REM 3. 执行 npm run docs:build，等价于 CI 的构建步骤
REM

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
echo Building BetaRobot docs...
echo.
call npm run docs:build
if errorlevel 1 (
    echo.
    echo Docs build failed
    pause
    exit /b 1
)

echo.
echo Docs build OK.
pause

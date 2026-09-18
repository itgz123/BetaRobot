@echo off
rem Bootstrap a BetaRobot workspace: clone the main repo, clone the app template
rem repo into the app folder, then write user_cfg.h so the build targets that app.
rem
rem Usage: setup_project.bat [main-repo-dir] [app-name]
rem        run with no arguments to be prompted for both.
rem
rem Prerequisite: SSH access to GitHub is configured (the repos are private).
rem
rem Keep this file ASCII-only: cmd reads it with the console codepage, and the
rem Chinese version of the generated header lives in user_cfg.h.example.

setlocal

set "MAIN_REPO_URL=git@github.com:itgz123/BetaRobot.git"
set "APP_REPO_URL=git@github.com:itgz123/BetaRobot-App-Example.git"
set "MAIN_DIR=%~1"
set "APP_NAME=%~2"

where git >nul 2>&1
if errorlevel 1 (
    echo ERROR: git not found. Install Git for Windows and add it to PATH.
    pause
    exit /b 1
)

if not defined MAIN_DIR set /p "MAIN_DIR=Main repo folder name [BetaRobot]: "
if not defined MAIN_DIR set "MAIN_DIR=BetaRobot"

if not defined APP_NAME set /p "APP_NAME=App name, cloned into the app folder [example]: "
if not defined APP_NAME set "APP_NAME=example"

set "APP_DIR=%MAIN_DIR%\app\%APP_NAME%"

echo.
echo === Main repo ===
if exist "%MAIN_DIR%\.git" (
    echo "%MAIN_DIR%" already exists - skipped
) else (
    echo Cloning main repo into "%MAIN_DIR%" ...
    git clone "%MAIN_REPO_URL%" "%MAIN_DIR%"
    if errorlevel 1 (
        echo ERROR: failed to clone the main repo. Check your SSH access to GitHub.
        pause
        exit /b 1
    )
)

echo.
echo === App repo ===
if exist "%APP_DIR%\.git" (
    echo "%APP_DIR%" already exists - skipped
) else (
    echo Cloning app template into "%APP_DIR%" ...
    if not exist "%MAIN_DIR%\app" mkdir "%MAIN_DIR%\app"
    git clone "%APP_REPO_URL%" "%APP_DIR%"
    if errorlevel 1 (
        echo ERROR: failed to clone the app repo. Check your SSH access to GitHub.
        pause
        exit /b 1
    )
)

echo.
echo === user_cfg.h ===
if exist "%MAIN_DIR%\user_cfg.h" (
    echo "%MAIN_DIR%\user_cfg.h" already exists - skipped
    echo Change APP_NAME in it to switch apps.
) else (
    echo Writing "%MAIN_DIR%\user_cfg.h" with APP_NAME=%APP_NAME%
    (
        echo #ifndef __USER_CFG_H
        echo #define __USER_CFG_H
        echo.
        echo /*============================================
        echo  *            Select the app to build
        echo  *     Corresponds to app/APP_NAME/
        echo  *     CMake reconfigures automatically after
        echo  *     the change; other apps keep their build
        echo  *     artifacts and need no rebuild.
        echo  *
        echo  *     Debug/Release come from the VSCode task or
        echo  *     cmake --build --config, not from this file.
        echo  *     Chinese template, with more comments:
        echo  *     user_cfg.h.example
        echo  *============================================*/
        echo #define APP_NAME %APP_NAME%
        echo.
        echo #endif // __USER_CFG_H
    ) > "%MAIN_DIR%\user_cfg.h"
)

echo.
echo Done. Next steps:
echo   cd %MAIN_DIR%
echo   cmake --preset default
echo   cmake --build --preset Debug
echo.
pause

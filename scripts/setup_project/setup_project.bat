@echo off
rem Bootstrap a BetaRobot workspace: clone the main repo, clone the app template
rem repo into the app folder, then write user_cfg.h so the build targets that app.
rem
rem Usage: setup_project.bat [main-repo-dir] [app-name]
rem        run with no arguments to be prompted for both.
rem
rem Both repos are public. Cloning channels are tried in order until one works:
rem   1. SSH      git@github.com:...                (needs a registered SSH key)
rem   2. SSH/443  ssh://git@ssh.github.com:443/...  (same key, for networks that block port 22)
rem   3. HTTPS    https://github.com/...            (no key needed)
rem   4. mirror   %BETAROBOT_GIT_MIRROR% + the HTTPS URL
rem When github.com is unreachable, set a mirror prefix (trailing slash optional)
rem and run again, e.g.:
rem   set BETAROBOT_GIT_MIRROR=https://ghfast.top/
rem   setup_project.bat
rem
rem Keep this file ASCII-only: cmd reads it with the console codepage, and the
rem Chinese version of the generated header lives in user_cfg.h.example.

setlocal enabledelayedexpansion

set "OWNER=itgz123"
set "MAIN_REPO_SLUG=%OWNER%/BetaRobot"
set "APP_REPO_SLUG=%OWNER%/BetaRobot-App-Example"
set "MAIN_DIR=%~1"
set "APP_NAME=%~2"
set "MIRROR=%BETAROBOT_GIT_MIRROR%"

rem Every channel has to fail fast: hanging on a password / host-key prompt leaves
rem no chance for the next channel. Respect an already configured GIT_SSH_COMMAND.
if not defined GIT_SSH_COMMAND set "GIT_SSH_COMMAND=ssh -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10"
set "GIT_TERMINAL_PROMPT=0"

rem Normalise the mirror prefix to exactly one trailing slash.
if defined MIRROR if "!MIRROR:~-1!"=="/" set "MIRROR=!MIRROR:~0,-1!"
if defined MIRROR set "MIRROR=!MIRROR!/"

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
    call :clone_repo "%MAIN_REPO_SLUG%" "%MAIN_DIR%"
    if errorlevel 1 (
        echo ERROR: failed to clone the main repo on every channel.
        echo        If github.com is unreachable, set BETAROBOT_GIT_MIRROR to a
        echo        working GitHub mirror prefix and run this script again.
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
    call :clone_repo "%APP_REPO_SLUG%" "%APP_DIR%"
    if errorlevel 1 (
        echo ERROR: failed to clone the app repo on every channel.
        echo        If github.com is unreachable, set BETAROBOT_GIT_MIRROR to a
        echo        working GitHub mirror prefix and run this script again.
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
exit /b 0

rem ---------------------------------------------------------------------------
rem :clone_repo <owner/name> <dest>
rem Walks the channels above; returns 0 as soon as one succeeds, 1 if all fail.
rem ---------------------------------------------------------------------------
:clone_repo
call :try_channel "git@github.com:%~1.git" "%~2"
if not errorlevel 1 exit /b 0
call :try_channel "ssh://git@ssh.github.com:443/%~1.git" "%~2"
if not errorlevel 1 exit /b 0
call :try_channel "https://github.com/%~1.git" "%~2"
if not errorlevel 1 exit /b 0
if defined MIRROR (
    call :try_channel "!MIRROR!https://github.com/%~1.git" "%~2"
    if not errorlevel 1 exit /b 0
)
exit /b 1

rem ---------------------------------------------------------------------------
rem :try_channel <url> <dest>
rem ---------------------------------------------------------------------------
:try_channel
echo     trying %~1
git clone "%~1" "%~2"
if not errorlevel 1 exit /b 0
rem A failed attempt leaves a half-cloned tree behind, which would make git
rem refuse the next channel ("destination path ... not an empty directory").
rem Only a .git-bearing dir can be ours here - the caller skipped real repos.
if exist "%~2\.git" rmdir /s /q "%~2"
exit /b 1

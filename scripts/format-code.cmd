@echo off
rem format-code.cmd - run clang-format in place over every .c/.h of the main repo.
rem
rem The file list comes from git ls-files, so:
rem   - the apps under app/ are separate git repos (the main repo does not
rem     descend into them), so they are naturally excluded;
rem   - anything ignored by .gitignore is naturally excluded;
rem   - cubemx/ (CubeMX-generated HAL code) is excluded explicitly.
rem Style lives in .clang-format at the repo root, shared with the
rem VS Code C/C++ extension.
rem
rem Usage: scripts\format-code.cmd
rem
rem Keep this file ASCII-only: cmd reads it with the console codepage.
setlocal

cd /d "%~dp0.." || (
    echo ERROR: Cannot find the repository root
    exit /b 1
)

where clang-format >nul 2>&1
if errorlevel 1 (
    echo ERROR: clang-format not found. Install LLVM/clang-format first.
    exit /b 1
)

set RC=0
for /f "delims=" %%f in ('git ls-files "*.c" "*.h" ":(exclude)cubemx/*"') do (
    clang-format -i -- "%%f"
    if errorlevel 1 set RC=1
)

if not "%RC%"=="0" (
    echo.
    echo format-code: some files failed
    exit /b 1
)

echo format-code: done
exit /b 0

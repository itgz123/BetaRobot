@echo off
REM ============================================================
REM PDF -> DOCX batch convert + extract
REM   For each PDF in tree:
REM     1. LibreOffice convert to docx      -> xxx.pdf.docx
REM     2. unzip docx(zip) into folder      -> xxx.pdf_extracted\
REM   Final result per pdf:
REM     source pdf + xxx.pdf.docx + xxx.pdf_extracted\ folder
REM
REM Usage:
REM   libreoffice_convert.bat             scan script's folder
REM   libreoffice_convert.bat <dir>       scan given folder
REM ============================================================
setlocal

set "DOCS_DIR=%~dp0"
if not "%~1"=="" set "DOCS_DIR=%~1"

REM locate LibreOffice CLI
set "SOFFICE=soffice.com"
where soffice.com >nul 2>nul
if errorlevel 1 set "SOFFICE=C:\Program Files\LibreOffice\program\soffice.com"

set /a count=0
set /a skipped=0
set /a failed=0

echo Scanning: %DOCS_DIR%

for /r "%DOCS_DIR%" %%f in (*.pdf) do call :process "%%~f"

echo.
echo Done: processed %count%, skipped %skipped%, failed %failed%
endlocal
exit /b

:process
set "NAME=%~nx1"
set "DOCX_FULL=%~1.docx"
set "DOCX_BASE=%~dpn1.docx"
set "DIR=%~1_extracted"

if exist "%DIR%" (
    if exist "%DOCX_FULL%" (
        echo [SKIP] %NAME%
        set /a skipped+=1
        goto :eof
    )
    if exist "%DOCX_BASE%" (
        move /y "%DOCX_BASE%" "%DOCX_FULL%" >nul 2>nul
        echo [SKIP] %NAME%
        set /a skipped+=1
        goto :eof
    )
)

echo [CONVERT] %NAME%
del /q "%DOCX_FULL%" "%DOCX_BASE%" 2>nul
"%SOFFICE%" --headless --infilter="writer_pdf_import" --convert-to "docx:Office Open XML Text" --outdir "%~dp1." "%~1"

if exist "%DOCX_BASE%" if not exist "%DOCX_FULL%" move /y "%DOCX_BASE%" "%DOCX_FULL%" >nul 2>nul
if not exist "%DOCX_FULL%" (
    echo [ERROR] convert failed: %NAME%
    set /a failed+=1
    goto :eof
)

echo [EXTRACT] %NAME%.docx
if exist "%DIR%" rmdir /s /q "%DIR%"
mkdir "%DIR%" 2>nul
"%SystemRoot%\System32\tar.exe" -xf "%DOCX_FULL%" -C "%DIR%"
set /a count+=1
goto :eof

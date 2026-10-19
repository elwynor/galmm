@echo off
rem Build 26.10.19.1 12:41PM
rem RUNTESTS.BAT - build and run Menu Magician's standalone tests.
rem Copyright (C) 2026 Elwynor Technologies.  Licensed under the AGPL-3.0.
rem
rem Needs the Visual Studio 2022 C++ tools (x86).  Run from any folder.

setlocal
set HERE=%~dp0
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat
if not exist "%VCVARS%" (
    echo RUNTESTS: vcvars32.bat not found - install the VS 2022 C++ tools
    exit /b 2
)
call "%VCVARS%" >nul
if not exist "%HERE%OUT" mkdir "%HERE%OUT"

rem The session runner (MMFIBER.C) against the stand-in engine headers.
cl /nologo /W3 /J /TC /D_CRT_SECURE_NO_WARNINGS /I"%HERE%FAKE" /I"%HERE%..\SRC\HVSMM" ^
   /Fo"%HERE%OUT\\" /Fe"%HERE%OUT\SESTEST.EXE" ^
   "%HERE%SESTEST.C" "%HERE%..\SRC\HVSMM\MMFIBER.C" || exit /b 1
"%HERE%OUT\SESTEST.EXE"
exit /b %ERRORLEVEL%
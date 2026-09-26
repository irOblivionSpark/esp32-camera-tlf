@echo off
cd /d "%~dp0"
set PORT=%1
if "%PORT%"=="" set PORT=COM5
where idf.py >nul 2>nul
if errorlevel 1 (echo Open an ESP-IDF PowerShell first.&pause&exit /b 1)
idf.py set-target esp32
if errorlevel 1 exit /b 1
idf.py build
if errorlevel 1 (echo BUILD FAILED.&pause&exit /b 1)
idf.py -p %PORT% flash
if errorlevel 1 (echo FLASH FAILED.&pause&exit /b 1)
idf.py -p %PORT% monitor

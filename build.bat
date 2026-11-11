@echo off
REM Build and run Placement Advisor with TinyCC (https://bellard.org/tcc/)
setlocal
cd /d "%~dp0"
where tcc 1>NUL 2>&1
if errorlevel 1 (
  echo TinyCC ^(tcc^) was not found on PATH. Download it from https://bellard.org/tcc/ and add it to PATH.
  exit /b 1
)
tcc src\placement_advisor.c -o placement_advisor.exe
if errorlevel 1 exit /b 1
echo Starting Placement Advisor at http://localhost:8080
start "" http://localhost:8080
placement_advisor.exe

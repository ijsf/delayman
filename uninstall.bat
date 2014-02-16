@echo off
SET DEBUGLOG="C:\DELAYMAN.LOG"

IF NOT X%_BUILDARCH%==X GOTO PERFORM

echo You must run this batch file from the WDK Build Environment.
GOTO QUIT

:PERFORM

echo ****************************************
echo **    Uninstalling driver             **
echo ****************************************
devcon remove Root\DelayMan
sc delete DelayMan
del %SystemRoot%\system32\drivers\delayman.sys >nul
del %SystemRoot%\system32\codelayman.dll >nul

IF NOT EXIST %DEBUGLOG% GOTO SKIPLOG
echo.
echo ****************************************
echo **    Installation debug log          **
echo ****************************************
TYPE %DEBUGLOG%
echo ****************************************
DEL %DEBUGLOG% >NUL
echo.

:SKIPLOG
echo.
echo Done.

:QUIT

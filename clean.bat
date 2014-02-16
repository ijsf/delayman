@echo off
IF NOT X%_BUILDARCH%==X GOTO PERFORM

echo You must run this batch file from the WDK Build Environment.
GOTO QUIT

:PERFORM

echo ****************************************
echo **    Cleaning directory              **
echo ****************************************
build -cqZ 2>nul
del /s /q obj
del /s /q *.log
rmdir /q /s obj

echo.
echo Done.

:QUIT

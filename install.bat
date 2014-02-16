@echo off
:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:: Configuration variables
:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

:: INF2CAT Windows version:
::       2000, XP_X86, XP_X64, Server2003_X86, Server2003_X64, Server2003_IA64,
::       Vista_X86, Vista_X64, Server2008_X86, Server2008_X64, Server2008_IA64

SET INFWINVER="Server2003_X86"

:: Object output path
SET OBJPATH="obj\i386"

:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:: Synchronize these with sign.bat
SET CERTSTORE="PrivateCertStore"
SET CERTSUBJECT=Bastage Inc. (Test)
SET CERTFILE="obj\testcert.cer"

SET DEBUGLOG="C:\DELAYMAN.LOG"

IF NOT X%_BUILDARCH%==X GOTO PERFORM

echo You must run this batch file from the WDK Build Environment.
GOTO QUIT

:PERFORM

echo ****************************************
echo **    Building DelayMan driver kit    **
echo ****************************************
build -c

echo.
echo ****************************************
echo **    Preparing files                 **
echo ****************************************
copy /y driver\delayman.inf %OBJPATH%
copy /y misc\* %OBJPATH%

echo.
echo ****************************************
echo **    Generating catalog file         **
echo ****************************************
inf2cat /driver:%OBJPATH% /os:%INFWINVER%

echo.
echo ****************************************
echo **    Signing catalog file            **
echo ****************************************
signtool sign /a /v /s %CERTSTORE% /n "%CERTSUBJECT%" /t http://timestamp.verisign.com/scripts/timestamp.dll %OBJPATH%\delayman.cat

echo.
echo ****************************************
echo **    Verifying catalog signature     **
echo ****************************************
signtool verify /v /pa %OBJPATH%\delayman.cat

echo.
echo ****************************************
echo **    Installing driver               **
echo ****************************************
CHOICE /M "Do you want to install the driver?"
IF ERRORLEVEL 2 GOTO SKIPINSTALL

echo.
devcon install %OBJPATH%\delayman.inf Root\DelayMan

echo.
echo You will have to reboot your computer in order for the changes to take effect.
echo.

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
:SKIPINSTALL
echo.
echo Done.

:QUIT

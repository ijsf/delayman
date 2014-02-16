@echo off
:: Synchronize these with install.bat
SET CERTSTORE="PrivateCertStore"
SET CERTSUBJECT=Bastage Inc. (Test)
SET CERTFILE="obj\testcert.cer"

IF NOT X%_BUILDARCH%==X GOTO PERFORM

echo You must run this batch file from the WDK Build Environment.
GOTO QUIT

:PERFORM

echo ****************************************
echo **    Generating test certificate     **
echo ****************************************
makecert -r -pe -ss %CERTSTORE% -n "CN=%CERTSUBJECT%" %CERTFILE%

echo.
echo ****************************************
echo **    Installing test certificate     **
echo ****************************************

echo.
echo Adding certificate to Trusted Root Certification Authorities
echo.
certmgr /add %CERTFILE% /s /r localMachine root

echo.
echo Adding certificate to Trusted Publishers
echo.
certmgr /add %CERTFILE% /s /r localMachine trustedpublisher

echo.
echo Done.

:QUIT

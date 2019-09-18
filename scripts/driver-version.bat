:: Copyright (c) 2019, Intel Corporation
::
:: Permission is hereby granted, free of charge, to any person obtaining a
:: copy of this software and associated documentation files (the "Software"),
:: to deal in the Software without restriction, including without limitation
:: the rights to use, copy, modify, merge, publish, distribute, sublicense,
:: and/or sell copies of the Software, and to permit persons to whom the
:: Software is furnished to do so, subject to the following conditions:
::
:: The above copyright notice and this permission notice shall be included
:: in all copies or substantial portions of the Software.
::
:: THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
:: OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
:: FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
:: THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
:: OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
:: ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
:: OTHER DEALINGS IN THE SOFTWARE.

@ECHO OFF

:: One parameter is expected
IF NOT B"%~2"==B"" (
    ECHO %0 called with no parameters, prints the version of the installed OpenCL driver
    ECHO %0 called with a single parameter containing expected version number,
    ECHO returns success ^(ERRORLEVEL=0^) if installed the specified driver version or newer
    ECHO returns failure ^(ERRORLEVEL=1^) if no driver or older than specified
    ECHO example:
    ECHO driver-version.bat 26.20.100.7158
    EXIT /B 1
)

SET DriverVersion=
FOR /F "tokens=3" %%D in ('WMIC path Win32_VideoController where AdapterCompatibility^="Intel Corporation" get AdapterCompatibility^, DriverVersion ^| findstr "Intel"') do (
    set DriverVersion=%%D
)
IF B"%DriverVersion%"==B"" (
    ECHO No driver detected in the system
    EXIT /B 1
)

IF B"%~1"==B"" (
    ECHO %DriverVersion%
    EXIT /B 1
)

FOR /F "delims=. tokens=1-4" %%A IN ("%DriverVersion%") DO (
    SET d1=%%A
    SET d2=%%B
    SET d3=%%C
    SET d4=%%D
)

FOR /F "delims=. tokens=1-4" %%A IN ("%~1") DO (
    SET p1=%%A
    SET p2=%%B
    SET p3=%%C
    SET p4=%%D
)

IF %d1% LSS %p1% GOTO FAIL
IF %d1% GTR %p1% GOTO PASS

IF %d2% LSS %p2% GOTO FAIL
IF %d2% GTR %p2% GOTO PASS

IF %d3% LSS %p3% GOTO FAIL
IF %d3% GTR %p3% GOTO PASS

IF %d4% LSS %p4% GOTO FAIL

:PASS
ECHO Driver %DriverVersion% is newer than or equal to referenced version passed from command line %1
EXIT /B 0

:FAIL
ECHO Driver %DriverVersion% is older than referenced from command line %1
EXIT /B 1

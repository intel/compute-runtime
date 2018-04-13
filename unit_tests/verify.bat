REM Copyright (c) 2017, Intel Corporation
REM
REM Permission is hereby granted, free of charge, to any person obtaining a
REM copy of this software and associated documentation files (the "Software"),
REM to deal in the Software without restriction, including without limitation
REM the rights to use, copy, modify, merge, publish, distribute, sublicense,
REM and/or sell copies of the Software, and to permit persons to whom the
REM Software is furnished to do so, subject to the following conditions:
REM
REM The above copyright notice and this permission notice shall be included
REM in all copies or substantial portions of the Software.
REM
REM THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
REM OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
REM FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
REM THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
REM OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
REM ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
REM OTHER DEALINGS IN THE SOFTWARE.
@where appverif
@if not "%ERRORLEVEL%"=="0" (
  @echo No appverif command.
  cmd /c exit /b 0
  set testError=0
  goto end
)

if exist ../../../../../terminate ( exit )
if exist ../../../../../no_verifier ( exit /b )

appverif.exe -enable Exceptions Handles Heaps Leak Locks Memory Threadpool TLS DirtyStacks -for igdrcl_tests.exe
%~dp0/igdrcl_tests.exe %*
set testError=%errorlevel%
echo App Verifier returned: %testError%
appverif.exe -disable * -for * > nul

:end
exit /b %testError%

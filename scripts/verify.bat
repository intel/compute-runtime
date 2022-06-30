::
:: Copyright (C) 2020-2021 Intel Corporation
::
:: SPDX-License-Identifier: MIT
::

@where appverif
@if not "%ERRORLEVEL%"=="0" (
  @echo No appverif command.
  cmd /c exit /b 0
  set testError=0
  goto end
)

appverif.exe -enable Exceptions Handles Heaps Leak Locks Memory Threadpool TLS DirtyStacks -for %1
%*
set testError=%errorlevel%
echo App Verifier returned: %testError%
appverif.exe -disable * -for * > nul

:end
exit /b %testError%

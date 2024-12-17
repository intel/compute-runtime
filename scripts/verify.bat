::
:: Copyright (C) 2020-2024 Intel Corporation
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

@if not "%NEO_APPVERIF_USER%"=="" (
  powershell -Command "$password = $Env:NEO_APPVERIF_PASS | ConvertTo-SecureString -AsPlainText -Force;$credential = [PSCredential]::New($Env:NEO_APPVERIF_USER,$password);Invoke-Command -ComputerName $(hostname) -Credential $credential -ScriptBlock {appverif.exe -enable Exceptions Handles Heaps Leak Locks Memory Threadpool TLS DirtyStacks -for %1}"
  %*
  set testError=%errorlevel%
  echo App Verifier returned: %testError%
  powershell -Command "$password = $Env:NEO_APPVERIF_PASS | ConvertTo-SecureString -AsPlainText -Force;$credential = [PSCredential]::New($Env:NEO_APPVERIF_USER,$password);Invoke-Command -ComputerName $(hostname) -Credential $credential -ScriptBlock {appverif.exe -disable * -for *}" > nul
) else (
  appverif.exe -enable Exceptions Handles Heaps Leak Locks Memory Threadpool TLS DirtyStacks -for %1
  %*
  set testError=%errorlevel%
  echo App Verifier returned: %testError%
  appverif.exe -disable * -for * > nul
)

:end
exit /b %testError%

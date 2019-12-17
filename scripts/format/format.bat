REM
REM Copyright (C) 2019 Intel Corporation
REM
REM SPDX-License-Identifier: MIT
REM

@echo off
setlocal EnableDelayedExpansion

IF NOT EXIST "%1" (
  echo Directory "%1" does not exist.
  exit /b 1
)

call clang-format --version
set err=%ERRORLEVEL%

if not "%err%"=="0" (
  echo clang-format not found
  exit /b 1
)

git --version
set err=%ERRORLEVEL%

if not "%err%"=="0" (
  echo git not found
  exit /b 1
)

pushd .
cd %1
git rev-parse --git-dir > NUL
set err=%ERRORLEVEL%

if not "%err%"=="0" (
  echo Not a git repository.
  exit /b 1
)

for /f %%i in ('git diff HEAD --name-only') do (
  set file="%%i"
  call :get_extension %%i
  call :test_extension !ext!
  if "!test!" == "1" (
    clang-format -i -style=file !file!
  )
)

popd
exit /b

:get_extension
set ext=%~x1
exit /b

:test_extension
set test=0
if "%1"==".h" set test=1
if "%1"==".cpp" set test=1
if "%1"==".inl" set test=1
exit /b

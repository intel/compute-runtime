/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "windows.h"
#include "excpt.h"
#include <string>

class SehException {
  public:
    static std::string getExceptionDescription(unsigned int code);
    static void getCallStack(unsigned int code, struct _EXCEPTION_POINTERS *ep, std::string &stack);
    static int filter(unsigned int code, struct _EXCEPTION_POINTERS *ep);

  protected:
    static void addLineToCallstack(std::string &callstack, size_t counter, std::string &module, std::string &line, std::string &symbol);
    typedef DWORD(WINAPI *getMappedFileNameFunction)(HANDLE hProcess, LPVOID lpv, LPSTR lpFilename, DWORD nSize);
};

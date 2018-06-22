/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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

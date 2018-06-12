/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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
#include "runtime/os_interface/os_library.h"

#define UMDF_USING_NTSTATUS
#include "runtime/os_interface/windows/windows_wrapper.h"

namespace OCLRT {
namespace Windows {

class OsLibrary : public OCLRT::OsLibrary {
  private:
    HMODULE handle;

  public:
    OsLibrary(const std::string &name);
    ~OsLibrary();

    bool isLoaded();
    void *getProcAddress(const std::string &procName);

  protected:
    HMODULE loadDependency(const std::string &dependencyFileName) const;

    static decltype(&LoadLibraryExA) loadLibraryExA;
    static decltype(&GetModuleFileNameA) getModuleFileNameA;
};
} // namespace Windows
} // namespace OCLRT

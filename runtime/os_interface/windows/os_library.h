/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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

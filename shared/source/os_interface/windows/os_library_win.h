/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_library.h"

#define UMDF_USING_NTSTATUS
#include "shared/source/os_interface/windows/windows_wrapper.h"

namespace NEO {
namespace Windows {

class OsLibrary : public NEO::OsLibrary {
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
} // namespace NEO

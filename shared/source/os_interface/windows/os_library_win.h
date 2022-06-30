/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_library.h"

#define UMDF_USING_NTSTATUS
#include <Windows.h>

namespace NEO {
namespace Windows {

class OsLibrary : public NEO::OsLibrary {
  private:
    HMODULE handle;

  public:
    OsLibrary(const std::string &name, std::string *errorValue);
    ~OsLibrary();

    bool isLoaded();
    void *getProcAddress(const std::string &procName);
    static decltype(&GetSystemDirectoryA) getSystemDirectoryA;
    void getLastErrorString(std::string *errorValue);

  protected:
    HMODULE loadDependency(const std::string &dependencyFileName) const;

    static decltype(&LoadLibraryExA) loadLibraryExA;
    static decltype(&GetModuleFileNameA) getModuleFileNameA;
};
} // namespace Windows
} // namespace NEO

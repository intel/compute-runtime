/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/os_library.h"

#define UMDF_USING_NTSTATUS
#include "shared/source/os_interface/windows/windows_wrapper.h"

namespace NEO {
namespace Windows {

class OsLibrary : public NEO::OsLibrary, NEO::NonCopyableAndNonMovableClass {
  private:
    HMODULE handle;

  public:
    OsLibrary(const OsLibraryCreateProperties &properties);
    ~OsLibrary();

    bool isLoaded();
    void *getProcAddress(const std::string &procName);
    static decltype(&GetSystemDirectoryA) getSystemDirectoryA;
    void getLastErrorString(std::string *errorValue);
    std::string getFullPath();

  protected:
    HMODULE loadDependency(const std::string &dependencyFileName) const;

    static decltype(&GetModuleHandleA) getModuleHandleA;
    static decltype(&LoadLibraryExA) loadLibraryExA;
    static decltype(&GetModuleFileNameA) getModuleFileNameA;
    static decltype(&FreeLibrary) freeLibrary;
    bool selfOpen = false;
};
} // namespace Windows
} // namespace NEO

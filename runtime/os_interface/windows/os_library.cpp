/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_library.h"
#include "os_library.h"

namespace OCLRT {

OsLibrary *OsLibrary::load(const std::string &name) {
    Windows::OsLibrary *ptr = new Windows::OsLibrary(name);

    if (!ptr->isLoaded()) {
        delete ptr;
        return nullptr;
    }
    return ptr;
}

namespace Windows {
decltype(&LoadLibraryExA) OsLibrary::loadLibraryExA = LoadLibraryExA;
decltype(&GetModuleFileNameA) OsLibrary::getModuleFileNameA = GetModuleFileNameA;

extern "C" IMAGE_DOS_HEADER __ImageBase;
__inline HINSTANCE GetModuleHINSTANCE() { return (HINSTANCE)&__ImageBase; }

HMODULE OsLibrary::loadDependency(const std::string &dependencyFileName) const {
    char dllPath[MAX_PATH];
    DWORD length = getModuleFileNameA(GetModuleHINSTANCE(), dllPath, MAX_PATH);
    for (DWORD idx = length; idx > 0; idx--) {
        if (dllPath[idx - 1] == '\\') {
            dllPath[idx] = '\0';
            break;
        }
    }
    strcat_s(dllPath, MAX_PATH, dependencyFileName.c_str());

    return loadLibraryExA(dllPath, NULL, 0);
}

OsLibrary::OsLibrary(const std::string &name) {
    if (name.empty()) {
        this->handle = GetModuleHandleA(nullptr);
    } else {
        this->handle = loadDependency(name);
        if (this->handle == nullptr) {
            this->handle = ::LoadLibraryA(name.c_str());
        }
    }
}

OsLibrary::~OsLibrary() {
    if ((this->handle != nullptr) && (this->handle != GetModuleHandleA(nullptr))) {
        ::FreeLibrary(this->handle);
        this->handle = nullptr;
    }
}

bool OsLibrary::isLoaded() {
    return this->handle != nullptr;
}

void *OsLibrary::getProcAddress(const std::string &procName) {
    return ::GetProcAddress(this->handle, procName.c_str());
}
} // namespace Windows
} // namespace OCLRT

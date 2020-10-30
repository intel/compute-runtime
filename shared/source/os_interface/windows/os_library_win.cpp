/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_library_win.h"

namespace NEO {

OsLibrary *OsLibrary::load(const std::string &name) {
    Windows::OsLibrary *ptr = new Windows::OsLibrary(name);

    if (!ptr->isLoaded()) {
        delete ptr;
        return nullptr;
    }
    return ptr;
}

const std::string OsLibrary::createFullSystemPath(const std::string &name) {
    CHAR buff[MAX_PATH];
    UINT ret = 0;
    ret = Windows::OsLibrary::getSystemDirectoryA(buff, MAX_PATH);
    buff[ret] = '\\';
    buff[ret + 1] = 0;
    strncat_s(&buff[0], sizeof(buff), name.c_str(), _TRUNCATE);
    return std::string(buff);
}

namespace Windows {
decltype(&LoadLibraryExA) OsLibrary::loadLibraryExA = LoadLibraryExA;
decltype(&GetModuleFileNameA) OsLibrary::getModuleFileNameA = GetModuleFileNameA;
decltype(&GetSystemDirectoryA) OsLibrary::getSystemDirectoryA = GetSystemDirectoryA;

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
} // namespace NEO

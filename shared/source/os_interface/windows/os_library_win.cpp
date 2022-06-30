/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_library_win.h"

namespace NEO {

OsLibrary *OsLibrary::load(const std::string &name) {
    return load(name, nullptr);
}

OsLibrary *OsLibrary::load(const std::string &name, std::string *errorValue) {
    Windows::OsLibrary *ptr = new Windows::OsLibrary(name, errorValue);

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

void OsLibrary::getLastErrorString(std::string *errorValue) {
    DWORD errorID = GetLastError();
    if (errorID && errorValue != nullptr) {

        LPSTR tempErrorMessage = nullptr;

        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, errorID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&tempErrorMessage, 0, NULL);

        errorValue->assign(tempErrorMessage);

        LocalFree(tempErrorMessage);
    }
}

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

OsLibrary::OsLibrary(const std::string &name, std::string *errorValue) {
    if (name.empty()) {
        this->handle = GetModuleHandleA(nullptr);
    } else {
        this->handle = loadDependency(name);
        if (this->handle == nullptr) {
            this->handle = ::LoadLibraryA(name.c_str());
            if ((this->handle == nullptr) && (errorValue != nullptr)) {
                getLastErrorString(errorValue);
            }
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

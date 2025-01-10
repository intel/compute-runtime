/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_library_win.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/windows/sys_calls.h"

#include <iomanip>
#include <regex>
#include <sstream>

namespace NEO {

OsLibrary *OsLibrary::load(const OsLibraryCreateProperties &properties) {
    Windows::OsLibrary *ptr = new Windows::OsLibrary(properties);

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
decltype(&GetModuleHandleA) OsLibrary::getModuleHandleA = GetModuleHandleA;
decltype(&LoadLibraryExA) OsLibrary::loadLibraryExA = LoadLibraryExA;
decltype(&GetModuleFileNameA) OsLibrary::getModuleFileNameA = GetModuleFileNameA;
decltype(&GetSystemDirectoryA) OsLibrary::getSystemDirectoryA = GetSystemDirectoryA;
decltype(&FreeLibrary) OsLibrary::freeLibrary = FreeLibrary;

extern "C" IMAGE_DOS_HEADER __ImageBase; // NOLINT(readability-identifier-naming)
__inline HINSTANCE getModuleHINSTANCE() { return (HINSTANCE)&__ImageBase; }

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
    DWORD length = getModuleFileNameA(getModuleHINSTANCE(), dllPath, MAX_PATH);
    for (DWORD idx = length; idx > 0; idx--) {
        if (dllPath[idx - 1] == '\\') {
            dllPath[idx] = '\0';
            break;
        }
    }
    strcat_s(dllPath, MAX_PATH, dependencyFileName.c_str());

    return loadLibraryExA(dllPath, NULL, 0);
}

OsLibrary::OsLibrary(const OsLibraryCreateProperties &properties) {
    if (properties.libraryName.empty()) {
        this->handle = getModuleHandleA(nullptr);
        this->selfOpen = true;
    } else {
        if (properties.performSelfLoad) {
            this->handle = getModuleHandleA(properties.libraryName.c_str());
            this->selfOpen = true;
        } else {
            this->handle = loadDependency(properties.libraryName);
            if (this->handle == nullptr) {
                this->handle = loadLibraryExA(properties.libraryName.c_str(), NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
            }
        }
        if ((this->handle == nullptr) && (properties.errorValue != nullptr)) {
            getLastErrorString(properties.errorValue);
        }
    }
}

OsLibrary::~OsLibrary() {
    if (!this->selfOpen && this->handle) {
        freeLibrary(this->handle);
        this->handle = nullptr;
    }
}

bool OsLibrary::isLoaded() {
    return this->handle != nullptr;
}

void *OsLibrary::getProcAddress(const std::string &procName) {
    return reinterpret_cast<void *>(::GetProcAddress(this->handle, procName.c_str()));
}

std::string OsLibrary::getFullPath() {
    char dllPath[MAX_PATH];
    getModuleFileNameA(getModuleHINSTANCE(), dllPath, MAX_PATH);
    return std::string(dllPath);
}
} // namespace Windows

bool getLoadedLibVersion(const std::string &libName, const std::string &regexVersionPattern, std::string &outVersion, std::string &errReason) {
    HMODULE mod = NULL;
    auto ret = SysCalls::getModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, std::wstring(libName.begin(), libName.end()).c_str(), &mod);
    if (0 == ret) {
        errReason = "Failed to read info of " + libName + " - GetModuleHandleExA failed, GetLastError=" + std::to_string(SysCalls::getLastError());
        return false;
    }

    wchar_t path[MAX_PATH];
    DWORD length = SysCalls::getModuleFileNameW(mod, path, MAX_PATH);
    if (0 == length) {
        errReason = "Failed to read info of " + libName + " - GetModuleFileName failed, GetLastError=" + std::to_string(SysCalls::getLastError());
        return false;
    }

    std::wstring trimmedPath = {path, length};
    DWORD infoVersioSize = SysCalls::getFileVersionInfoSizeW(trimmedPath.c_str(), nullptr);
    if (0 == infoVersioSize) {
        errReason = "Failed to read info of " + libName + " - GetFileVersionInfoSize failed, GetLastError=" + std::to_string(SysCalls::getLastError());
        return false;
    }

    std::vector<char> fileInformationBackingStorage;
    fileInformationBackingStorage.resize(infoVersioSize);
    ret = SysCalls::getFileVersionInfoW(path, 0, static_cast<DWORD>(fileInformationBackingStorage.size()), fileInformationBackingStorage.data());
    if (0 == ret) {
        errReason = "Failed to read info of " + libName + " - GetFileVersionInfo failed, GetLastError=" + std::to_string(SysCalls::getLastError());
        return false;
    }

    struct LangCodePage {
        WORD lang;
        WORD codePage;
    };

    LangCodePage *translateInfo = nullptr;

    unsigned int langCodePagesSize = 0;
    ret = SysCalls::verQueryValueW(fileInformationBackingStorage.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<LPVOID *>(&translateInfo), &langCodePagesSize);
    if (0 == ret) {
        errReason = "Failed to read info of " + libName + " - VerQueryValue(\\VarFileInfo\\Translation) failed, GetLastError=" + std::to_string(SysCalls::getLastError());
        return false;
    }

    auto truncateWstringToString = [](const std::wstring &ws) {
        std::string ret;
        std::transform(ws.begin(), ws.end(), std::back_inserter(ret), [](wchar_t wc) { return static_cast<char>(wc); });
        return ret;
    };

    size_t langCodePagesCount = (langCodePagesSize / sizeof(LangCodePage));
    std::regex versionPattern{regexVersionPattern};
    for (size_t j = 0; j < langCodePagesCount; ++j) {
        std::wstringstream subBlockPath;
        subBlockPath << L"\\StringFileInfo\\";
        subBlockPath << std::setw(4) << std::setfill(L'0') << std::hex << translateInfo[j].lang;
        subBlockPath << std::setw(4) << std::setfill(L'0') << std::hex << translateInfo[j].codePage;
        subBlockPath << L"\\ProductVersion";

        wchar_t *data;
        unsigned int len = 0;
        ret = SysCalls::verQueryValueW(fileInformationBackingStorage.data(), subBlockPath.str().c_str(), (LPVOID *)&data, &len);
        if (0 == ret) {
            errReason = "Failed to read info of " + libName + " - VerQueryValue(" + truncateWstringToString(subBlockPath.str()) + ") failed, GetLastError=" + std::to_string(SysCalls::getLastError());
            return false;
        }

        auto sdata = truncateWstringToString(data);
        if (std::regex_search(sdata, versionPattern)) {
            outVersion = sdata;
            return true;
        }
    }

    errReason = "Could not find version info for " + std::string(libName) + " that would satisfy expected pattern\n";
    return false;
}
} // namespace NEO

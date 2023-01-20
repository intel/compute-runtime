/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sys_calls.h"

#include "shared/test/common/os_interface/windows/mock_sys_calls.h"

#include <cstdint>

namespace NEO {

namespace SysCalls {

unsigned int getProcessId() {
    return 0xABCEDF;
}

unsigned long getNumThreads() {
    return 1;
}

BOOL systemPowerStatusRetVal = 1;
BYTE systemPowerStatusACLineStatusOverride = 1;
const wchar_t *currentLibraryPath = L"";
uint32_t regOpenKeySuccessCount = 0u;
uint32_t regQueryValueSuccessCount = 0u;
uint64_t regQueryValueExpectedData = 0ull;
const HKEY validHkey = reinterpret_cast<HKEY>(0);
bool getNumThreadsCalled = false;

HANDLE createEvent(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName) {
    if (mockCreateEventClb) {
        return mockCreateEventClb(lpEventAttributes, bManualReset, bInitialState, lpName, mockCreateEventClbData);
    }
    return reinterpret_cast<HANDLE>(dummyHandle);
}

BOOL closeHandle(HANDLE hObject) {
    if (mockCloseHandleClb) {
        return mockCloseHandleClb(hObject, mockCloseHandleClbData);
    }
    return (reinterpret_cast<HANDLE>(dummyHandle) == hObject) ? TRUE : FALSE;
}

BOOL getSystemPowerStatus(LPSYSTEM_POWER_STATUS systemPowerStatusPtr) {
    systemPowerStatusPtr->ACLineStatus = systemPowerStatusACLineStatusOverride;
    return systemPowerStatusRetVal;
}
BOOL getModuleHandle(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE *phModule) {
    constexpr auto expectedFlags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (dwFlags != expectedFlags) {
        return FALSE;
    }
    *phModule = handleValue;
    return TRUE;
}
DWORD getModuleFileName(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) {
    lstrcpyW(lpFilename, currentLibraryPath);
    return TRUE;
}

char *openCLDriverName = "igdrcl.dll";

char *getenv(const char *variableName) {
    if (strcmp(variableName, "OpenCLDriverName") == 0) {
        return openCLDriverName;
    }
    return ::getenv(variableName);
}

LSTATUS regOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) {
    if (regOpenKeySuccessCount > 0) {
        regOpenKeySuccessCount--;
        if (phkResult) {
            *phkResult = validHkey;
        }
        return ERROR_SUCCESS;
    }
    return ERROR_FILE_NOT_FOUND;
};

LSTATUS regQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    if (hKey == validHkey && regQueryValueSuccessCount > 0) {
        regQueryValueSuccessCount--;

        if (lpcbData) {
            if (strcmp(lpValueName, "settingSourceString") == 0) {
                const auto settingSource = "registry";
                if (lpData) {
                    strcpy(reinterpret_cast<char *>(lpData), settingSource);
                } else {
                    *lpcbData = static_cast<DWORD>(strlen(settingSource) + 1u);
                    if (lpType) {
                        *lpType = REG_SZ;
                    }
                }
            } else if (strcmp(lpValueName, "settingSourceInt") == 0) {
                if (lpData) {
                    *reinterpret_cast<DWORD *>(lpData) = 1;
                } else {
                    *lpcbData = sizeof(DWORD);
                }
            } else if (strcmp(lpValueName, "settingSourceInt64") == 0) {
                if (lpData) {
                    *reinterpret_cast<INT64 *>(lpData) = 0xffffffffeeeeeeee;
                } else {
                    *lpcbData = sizeof(INT64);
                }
            } else if (strcmp(lpValueName, "settingSourceBinary") == 0) {
                const auto settingSource = L"registry";
                auto size = wcslen(settingSource) * sizeof(wchar_t);
                if (lpData) {
                    memcpy(reinterpret_cast<wchar_t *>(lpData), settingSource, size);
                } else {
                    *lpcbData = static_cast<DWORD>(size);
                    if (lpType) {
                        *lpType = REG_BINARY;
                    }
                }
            } else if (strcmp(lpValueName, "boolRegistryKey") == 0) {
                if (*lpcbData == sizeof(int64_t)) {
                    if (lpData) {
                        *reinterpret_cast<uint64_t *>(lpData) = regQueryValueExpectedData;
                    }
                }
            }
        }

        return ERROR_SUCCESS;
    }
    return ERROR_FILE_NOT_FOUND;
};
} // namespace SysCalls

bool isShutdownInProgress() {
    return false;
}

unsigned int getPid() {
    return 0xABCEDF;
}

unsigned int readEnablePreemptionRegKey() {
    return 1;
}

} // namespace NEO

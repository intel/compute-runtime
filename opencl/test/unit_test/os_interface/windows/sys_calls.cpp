/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sys_calls.h"

#include "opencl/test/unit_test/os_interface/windows/mock_sys_calls.h"

namespace NEO {

namespace SysCalls {

unsigned int getProcessId() {
    return 0xABCEDF;
}

BOOL systemPowerStatusRetVal = 1;
BYTE systemPowerStatusACLineStatusOverride = 1;
const wchar_t *currentLibraryPath = L"";

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

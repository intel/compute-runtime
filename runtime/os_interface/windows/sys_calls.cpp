/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sys_calls.h"

namespace NEO {

namespace SysCalls {

HANDLE createEvent(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName) {
    return CreateEventA(lpEventAttributes, bManualReset, bInitialState, lpName);
}

BOOL closeHandle(HANDLE hObject) {
    return CloseHandle(hObject);
}

BOOL getSystemPowerStatus(LPSYSTEM_POWER_STATUS systemPowerStatusPtr) {
    return GetSystemPowerStatus(systemPowerStatusPtr);
}
BOOL getModuleHandle(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE *phModule) {
    return GetModuleHandleEx(dwFlags, lpModuleName, phModule);
}
DWORD getModuleFileName(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) {
    return GetModuleFileName(hModule, lpFilename, nSize);
}
} // namespace SysCalls

} // namespace NEO

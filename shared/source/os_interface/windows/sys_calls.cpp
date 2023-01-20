/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/sys_calls.h"

namespace NEO {

unsigned int getPid() {
    return GetCurrentProcessId();
}

bool isShutdownInProgress() {
    auto handle = GetModuleHandleA("ntdll.dll");

    if (!handle) {
        return true;
    }

    auto RtlDllShutdownInProgress = reinterpret_cast<BOOLEAN(WINAPI *)()>(GetProcAddress(handle, "RtlDllShutdownInProgress"));
    return RtlDllShutdownInProgress();
}

namespace SysCalls {

unsigned int getProcessId() {
    return GetCurrentProcessId();
}

unsigned long getNumThreads() {
    return 1;
}

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
char *getenv(const char *variableName) {
    return ::getenv(variableName);
}

LSTATUS regOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) {
    return RegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

LSTATUS regQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    return RegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}
} // namespace SysCalls

} // namespace NEO

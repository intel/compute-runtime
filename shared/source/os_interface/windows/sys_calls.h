/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <Windows.h>

namespace NEO {

namespace SysCalls {

HANDLE createEvent(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName);
BOOL closeHandle(HANDLE hObject);
BOOL getSystemPowerStatus(LPSYSTEM_POWER_STATUS systemPowerStatusPtr);
BOOL getModuleHandle(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE *phModule);
DWORD getModuleFileName(HMODULE hModule, LPWSTR lpFilename, DWORD nSize);
char *getenv(const char *variableName);
BOOL enumDisplayDevices(LPCWSTR lpDevice, DWORD iDevNum, PDISPLAY_DEVICEW lpDisplayDevice, DWORD dwFlags);
} // namespace SysCalls

} // namespace NEO

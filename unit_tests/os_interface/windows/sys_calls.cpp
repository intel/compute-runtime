/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sys_calls.h"

namespace OCLRT {

namespace SysCalls {

constexpr uintptr_t dummyHandle = static_cast<uintptr_t>(0x7);

BOOL systemPowerStatusRetVal = 1;
BYTE systemPowerStatusACLineStatusOverride = 1;

HANDLE createEvent(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName) {
    return reinterpret_cast<HANDLE>(dummyHandle);
}

BOOL closeHandle(HANDLE hObject) {
    return (reinterpret_cast<HANDLE>(dummyHandle) == hObject) ? TRUE : FALSE;
}

BOOL getSystemPowerStatus(LPSYSTEM_POWER_STATUS systemPowerStatusPtr) {
    systemPowerStatusPtr->ACLineStatus = systemPowerStatusACLineStatusOverride;
    return systemPowerStatusRetVal;
}

} // namespace SysCalls

} // namespace OCLRT

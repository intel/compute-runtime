/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/windows/ult_dxcore_factory.h"

namespace NEO {

HRESULT WINAPI ULTDXCoreCreateAdapterFactory(REFIID riid, void **ppFactory) {
    *reinterpret_cast<UltDXCoreAdapterFactory **>(ppFactory) = new UltDXCoreAdapterFactory;
    return S_OK;
}

void WINAPI ULTGetSystemInfo(SYSTEM_INFO *pSystemInfo) {
    pSystemInfo->lpMaximumApplicationAddress = is32bit ? (LPVOID)MemoryConstants::max32BitAppAddress : (LPVOID)MemoryConstants::max64BitAppAddress;
}

const char *UltDxCoreAdapter::description = "Intel";
bool UltDXCoreAdapterList::firstInvalid = false;

extern uint32_t numRootDevicesToEnum = 1;

} // namespace NEO

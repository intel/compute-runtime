/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/windows/ult_dxcore_factory.h"

#include "shared/source/helpers/constants.h"

namespace NEO {

HRESULT WINAPI ultDxCoreCreateAdapterFactory(REFIID riid, void **ppFactory) {
    *reinterpret_cast<UltDXCoreAdapterFactory **>(ppFactory) = new UltDXCoreAdapterFactory;
    return S_OK;
}

void WINAPI ultGetSystemInfo(SYSTEM_INFO *pSystemInfo) {
#ifdef _WIN32
    pSystemInfo->lpMaximumApplicationAddress = is32bit ? (LPVOID)MemoryConstants::max32BitAppAddress : (LPVOID)MemoryConstants::max64BitAppAddress;
#endif
}

const char *UltDxCoreAdapter::description = "Intel";
bool UltDXCoreAdapterList::firstInvalid = false;

uint32_t numRootDevicesToEnum = 1;

} // namespace NEO

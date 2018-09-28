/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/memory_constants.h"
#include "unit_tests/os_interface/windows/ult_dxgi_factory.h"

namespace OCLRT {

HRESULT WINAPI ULTCreateDXGIFactory(REFIID riid, void **ppFactory) {

    UltIDXGIFactory1 *factory = new UltIDXGIFactory1;

    *(UltIDXGIFactory1 **)ppFactory = factory;

    return S_OK;
}

void WINAPI ULTGetSystemInfo(SYSTEM_INFO *pSystemInfo) {
    pSystemInfo->lpMaximumApplicationAddress = is32bit ? (LPVOID)MemoryConstants::max32BitAppAddress : (LPVOID)MemoryConstants::max64BitAppAddress;
}

} // namespace OCLRT

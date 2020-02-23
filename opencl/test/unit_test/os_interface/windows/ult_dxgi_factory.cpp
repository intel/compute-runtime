/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/os_interface/windows/ult_dxgi_factory.h"

namespace NEO {

HRESULT WINAPI ULTCreateDXGIFactory(REFIID riid, void **ppFactory) {

    UltIDXGIFactory1 *factory = new UltIDXGIFactory1;

    *(UltIDXGIFactory1 **)ppFactory = factory;

    return S_OK;
}

void WINAPI ULTGetSystemInfo(SYSTEM_INFO *pSystemInfo) {
    pSystemInfo->lpMaximumApplicationAddress = is32bit ? (LPVOID)MemoryConstants::max32BitAppAddress : (LPVOID)MemoryConstants::max64BitAppAddress;
}

const wchar_t *UltIDXGIAdapter1::description = L"Intel";

} // namespace NEO

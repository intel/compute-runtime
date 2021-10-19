/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/windows/ult_dxgi_factory.h"

namespace NEO {

HRESULT WINAPI ULTCreateDXGIFactory(REFIID riid, void **ppFactory) {

    UltIDXGIFactory1 *factory = new UltIDXGIFactory1;

    *(UltIDXGIFactory1 **)ppFactory = factory;

    return S_OK;
}

const wchar_t *UltIDXGIAdapter1::description = L"Intel";

} // namespace NEO

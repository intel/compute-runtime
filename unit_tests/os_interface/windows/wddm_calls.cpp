/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_wddm.h"
#include "unit_tests/os_interface/windows/ult_dxgi_factory.h"

namespace OCLRT {

BOOL WINAPI ULTVirtualFree(LPVOID ptr, SIZE_T size, DWORD flags) {
    free(ptr);
    return 1;
}

LPVOID WINAPI ULTVirtualAlloc(LPVOID inPtr, SIZE_T size, DWORD flags, DWORD type) {
    return malloc(size);
}

Wddm::CreateDXGIFactoryFcn getCreateDxgiFactory() {
    return ULTCreateDXGIFactory;
}

Wddm::GetSystemInfoFcn getGetSystemInfo() {
    return ULTGetSystemInfo;
}

Wddm::VirtualFreeFcn getVirtualFree() {
    return ULTVirtualFree;
}

Wddm::VirtualAllocFcn getVirtualAlloc() {
    return ULTVirtualAlloc;
}
} // namespace OCLRT

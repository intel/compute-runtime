/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/host_ptr_defines.h"

#include "opencl/test/unit_test/mocks/mock_wddm.h"
#include "opencl/test/unit_test/os_interface/windows/ult_dxgi_factory.h"

namespace NEO {

BOOL WINAPI ULTVirtualFree(LPVOID ptr, SIZE_T size, DWORD flags) {
    return 1;
}

LPVOID WINAPI ULTVirtualAlloc(LPVOID inPtr, SIZE_T size, DWORD flags, DWORD type) {
    return reinterpret_cast<LPVOID>(virtualAllocAddress);
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
} // namespace NEO

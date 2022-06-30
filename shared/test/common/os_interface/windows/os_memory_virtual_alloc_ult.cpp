/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_memory_win.h"
#include "shared/test/common/mocks/mock_wddm.h"

namespace NEO {

BOOL WINAPI ULTVirtualFree(LPVOID ptr, SIZE_T size, DWORD flags) {
    return 1;
}

LPVOID WINAPI ULTVirtualAlloc(LPVOID inPtr, SIZE_T size, DWORD flags, DWORD type) {
    return reinterpret_cast<LPVOID>(virtualAllocAddress);
}

OSMemoryWindows::VirtualFreeFcn getVirtualFree() {
    return ULTVirtualFree;
}

OSMemoryWindows::VirtualAllocFcn getVirtualAlloc() {
    return ULTVirtualAlloc;
}
} // namespace NEO

/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/windows/os_memory_win.h"
#include "shared/test/common/mocks/mock_wddm.h"

namespace NEO {

namespace SysCalls {
extern bool mmapAllowExtendedPointers;
}

BOOL WINAPI ultVirtualFree(LPVOID ptr, SIZE_T size, DWORD flags) {
    return 1;
}

LPVOID WINAPI ultVirtualAlloc(LPVOID inPtr, SIZE_T size, DWORD flags, DWORD type) {
    if (castToUint64(inPtr) > maxNBitValue(48) && SysCalls::mmapAllowExtendedPointers) {
        return inPtr;
    }
    return reinterpret_cast<LPVOID>(virtualAllocAddress);
}

OSMemoryWindows::VirtualFreeFcn getVirtualFree() {
    return ultVirtualFree;
}

OSMemoryWindows::VirtualAllocFcn getVirtualAlloc() {
    return ultVirtualAlloc;
}
} // namespace NEO

/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_memory_win.h"

namespace NEO {

std::unique_ptr<OSMemory> OSMemory::create() {
    return std::make_unique<OSMemoryWindows>();
}

void *OSMemoryWindows::reserveCpuAddressRange(size_t sizeToReserve) {
    return virtualAllocWrapper(0, sizeToReserve, MEM_RESERVE, PAGE_READWRITE);
}

void OSMemoryWindows::releaseCpuAddressRange(void *reservedCpuAddressRange, size_t /* reservedSize */) {
    virtualFreeWrapper(reservedCpuAddressRange, 0, MEM_RELEASE);
}

LPVOID OSMemoryWindows::virtualAllocWrapper(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
    return VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);
}

BOOL OSMemoryWindows::virtualFreeWrapper(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
    return VirtualFree(lpAddress, dwSize, dwFreeType);
}

} // namespace NEO

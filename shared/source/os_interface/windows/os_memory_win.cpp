/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_memory_win.h"

namespace NEO {

extern OSMemoryWindows::VirtualAllocFcn getVirtualAlloc();
extern OSMemoryWindows::VirtualFreeFcn getVirtualFree();

OSMemoryWindows::VirtualAllocFcn OSMemoryWindows::virtualAllocFnc = getVirtualAlloc();
OSMemoryWindows::VirtualFreeFcn OSMemoryWindows::virtualFreeFnc = getVirtualFree();

std::unique_ptr<OSMemory> OSMemory::create() {
    return std::make_unique<OSMemoryWindows>();
}

void *OSMemoryWindows::osReserveCpuAddressRange(void *baseAddress, size_t sizeToReserve, bool topDownHint) {
    auto flags = MEM_RESERVE | (topDownHint ? MEM_TOP_DOWN : 0);
    return virtualAllocWrapper(baseAddress, sizeToReserve, flags, PAGE_READWRITE);
}

void OSMemoryWindows::osReleaseCpuAddressRange(void *reservedCpuAddressRange, size_t size) {
    virtualFreeWrapper(reservedCpuAddressRange, size, MEM_RELEASE);
}

LPVOID OSMemoryWindows::virtualAllocWrapper(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
    return this->virtualAllocFnc(lpAddress, dwSize, flAllocationType, flProtect);
}

BOOL OSMemoryWindows::virtualFreeWrapper(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
    return this->virtualFreeFnc(lpAddress, dwSize, dwFreeType);
}

} // namespace NEO

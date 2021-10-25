/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_memory.h"

#include <windows.h>

namespace NEO {

class OSMemoryWindows : public OSMemory {
  public:
    OSMemoryWindows() = default;

    void getMemoryMaps(MemoryMaps &memoryMaps) override {}

    typedef BOOL(WINAPI *VirtualFreeFcn)(LPVOID ptr, SIZE_T size, DWORD flags);
    typedef LPVOID(WINAPI *VirtualAllocFcn)(LPVOID inPtr, SIZE_T size, DWORD flags, DWORD type);

  protected:
    void *osReserveCpuAddressRange(void *baseAddress, size_t sizeToReserve, bool topDownHint) override;
    void osReleaseCpuAddressRange(void *reservedCpuAddressRange, size_t reservedSize) override;

    MOCKABLE_VIRTUAL LPVOID virtualAllocWrapper(LPVOID, SIZE_T, DWORD, DWORD);
    MOCKABLE_VIRTUAL BOOL virtualFreeWrapper(LPVOID, SIZE_T, DWORD);

    static VirtualFreeFcn virtualFreeFnc;
    static VirtualAllocFcn virtualAllocFnc;
};

}; // namespace NEO

/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_memory.h"

#include <windows.h>

namespace NEO {

class OSMemoryWindows : public OSMemory {
  public:
    OSMemoryWindows() = default;
    void *reserveCpuAddressRange(size_t sizeToReserve) override;
    void releaseCpuAddressRange(void *reservedCpuAddressRange, size_t reservedSize) override;

  protected:
    MOCKABLE_VIRTUAL LPVOID virtualAllocWrapper(LPVOID, SIZE_T, DWORD, DWORD);
    MOCKABLE_VIRTUAL BOOL virtualFreeWrapper(LPVOID, SIZE_T, DWORD);
};

}; // namespace NEO

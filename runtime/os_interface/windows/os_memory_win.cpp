/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_memory.h"

#include <windows.h>

namespace NEO {

void *OSMemory::reserveCpuAddressRange(size_t sizeToReserve) {
    return VirtualAlloc(0, sizeToReserve, MEM_RESERVE, PAGE_READWRITE);
}

void OSMemory::releaseCpuAddressRange(void *reservedCpuAddressRange, size_t /* reservedSize */) {
    VirtualFree(reservedCpuAddressRange, 0, MEM_RELEASE);
}

} // namespace NEO

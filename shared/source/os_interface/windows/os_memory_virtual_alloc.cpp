/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_memory_win.h"

namespace NEO {

OSMemoryWindows::VirtualFreeFcn getVirtualFree() {
    return VirtualFree;
}

OSMemoryWindows::VirtualAllocFcn getVirtualAlloc() {
    return VirtualAlloc;
}
} // namespace NEO

/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_memory_manager.h"

#include <winnt.h>

namespace NEO {
constexpr uint32_t pageWriteCombine = 0x400;

bool WddmMemoryManager::isWCMemory(const void *ptr) {
    MEMORY_BASIC_INFORMATION info;
    VirtualQuery(ptr, &info, sizeof(info));
    return info.AllocationProtect & pageWriteCombine;
}
} // namespace NEO
/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"

#include <winnt.h>

namespace NEO {
constexpr uint32_t pageWriteCombine = 0x400;

bool WddmMemoryManager::isWCMemory(const void *ptr) {
    MEMORY_BASIC_INFORMATION info;
    VirtualQuery(ptr, &info, sizeof(info));
    return info.AllocationProtect & pageWriteCombine;
}

bool WddmMemoryManager::isHostMemoryMappable(const void *ptr, size_t size, bool writable) {
    constexpr DWORD writableProtection = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    constexpr DWORD readableProtection = writableProtection | PAGE_READONLY | PAGE_EXECUTE_READ;
    const DWORD requiredProtection = writable ? writableProtection : readableProtection;

    auto address = reinterpret_cast<uintptr_t>(ptr);
    const auto end = address + size;
    if (end < address) {
        return false;
    }
    while (address < end) {
        MEMORY_BASIC_INFORMATION info = {};
        if (SysCalls::virtualQuery(reinterpret_cast<LPCVOID>(address), &info, sizeof(info)) != sizeof(info) ||
            info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) || !(info.Protect & requiredProtection)) {
            return false;
        }
        address = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    }
    return true;
}
} // namespace NEO

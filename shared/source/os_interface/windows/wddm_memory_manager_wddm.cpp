/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/allocation_properties.h"
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

AddressRange WddmMemoryManager::reserveCpuAddress(const uint64_t requiredStartAddress, size_t size) {
    void *ptr = SysCalls::virtualAlloc2(GetCurrentProcess(), addrToPtr(requiredStartAddress), size, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS);
    return {castToUint64(ptr), size};
}

void WddmMemoryManager::freeCpuAddress(AddressRange addressRange) {
    auto address = addrToPtr(addressRange.address);
    // Mapped views split the placeholder, which can be released only as a whole; coalescing fails on an unsplit one.
    SysCalls::virtualFree(address, addressRange.size, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS);
    [[maybe_unused]] auto releaseStatus = SysCalls::virtualFree(address, 0u, MEM_RELEASE);
    DEBUG_BREAK_IF(!releaseStatus);
}

GraphicsAllocation *WddmMemoryManager::allocatePhysicalHostMemory(const AllocationData &allocationData, AllocationStatus &status) {
    status = AllocationStatus::Error;

    auto section = SysCalls::createFileMapping2(INVALID_HANDLE_VALUE, nullptr, FILE_MAP_READ | FILE_MAP_WRITE, PAGE_READWRITE, SEC_COMMIT, allocationData.size, nullptr);
    if (section == nullptr) {
        return nullptr;
    }

    auto view = SysCalls::mapViewOfFile3(section, GetCurrentProcess(), nullptr, 0u, allocationData.size, 0u, PAGE_READWRITE);
    if (view == nullptr) {
        SysCalls::closeHandle(section);
        return nullptr;
    }

    auto allocation = this->createPhysicalHostAllocation(allocationData, view);
    if (allocation == nullptr) {
        SysCalls::unmapViewOfFile(view);
        SysCalls::closeHandle(section);
        return nullptr;
    }

    allocation->setSectionHandle(section);
    allocation->setSectionView(view);
    status = AllocationStatus::Success;
    return allocation.release();
}

bool WddmMemoryManager::mapPhysicalHostMemoryToCpuAddress(const WddmAllocation &physicalAllocation, WddmAllocation &mappedAllocation, size_t offset) {
    auto placeholder = mappedAllocation.getReservedAddressPtr();
    auto size = mappedAllocation.getReservedAddressSize();
    MEMORY_BASIC_INFORMATION firstPlaceholder = {};
    MEMORY_BASIC_INFORMATION lastPlaceholder = {};
    SysCalls::virtualQuery(placeholder, &firstPlaceholder, sizeof(firstPlaceholder));
    SysCalls::virtualQuery(ptrOffset(placeholder, size - 1), &lastPlaceholder, sizeof(lastPlaceholder));
    auto placeholdersEnd = ptrOffset(lastPlaceholder.BaseAddress, lastPlaceholder.RegionSize);
    // Earlier mappings may split the range into several placeholders, but a view replaces exactly one. Merging and
    // splitting fail when there is nothing to merge or split; mapping the view checks the result.
    SysCalls::virtualFree(firstPlaceholder.AllocationBase, ptrDiff(placeholdersEnd, firstPlaceholder.AllocationBase), MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS);
    SysCalls::virtualFree(placeholder, size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
    if (SysCalls::mapViewOfFile3(physicalAllocation.getSectionHandle(), GetCurrentProcess(), placeholder, offset, size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE) == nullptr) {
        return false;
    }
    mappedAllocation.setPlaceholderViewMapped(true);
    return true;
}

void WddmMemoryManager::unmapPhysicalHostMemoryFromCpuAddress(WddmAllocation &mappedAllocation) {
    if (!mappedAllocation.isPlaceholderViewMapped()) {
        return;
    }
    [[maybe_unused]] auto unmapStatus = SysCalls::unmapViewOfFile2(GetCurrentProcess(), mappedAllocation.getReservedAddressPtr(), MEM_PRESERVE_PLACEHOLDER);
    DEBUG_BREAK_IF(!unmapStatus);
    mappedAllocation.setPlaceholderViewMapped(false);
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

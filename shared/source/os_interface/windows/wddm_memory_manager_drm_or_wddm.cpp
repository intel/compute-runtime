/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"

namespace NEO {
bool WddmMemoryManager::isWCMemory(const void *ptr) {
    return false;
}

AddressRange WddmMemoryManager::reserveCpuAddress(const uint64_t requiredStartAddress, size_t size) {
    void *ptr = osMemory->osReserveCpuAddressRange(addrToPtr(requiredStartAddress), size, false);
    return {castToUint64(ptr), size};
}

void WddmMemoryManager::freeCpuAddress(AddressRange addressRange) {
    osMemory->osReleaseCpuAddressRange(addrToPtr(addressRange.address), addressRange.size);
}

GraphicsAllocation *WddmMemoryManager::allocatePhysicalHostMemory(const AllocationData &allocationData, AllocationStatus &status) {
    status = AllocationStatus::Error;

    auto allocation = this->createPhysicalHostAllocation(allocationData, nullptr);
    if (allocation == nullptr) {
        return nullptr;
    }

    auto cpuPtr = lockResource(allocation.get());
    if (!cpuPtr) {
        destroyAllocationHandles(*allocation);
        return nullptr;
    }

    allocation->setCpuAddress(cpuPtr);
    status = AllocationStatus::Success;
    return allocation.release();
}

bool WddmMemoryManager::mapPhysicalHostMemoryToCpuAddress(const WddmAllocation &physicalAllocation, WddmAllocation &mappedAllocation, size_t offset) {
    return true;
}

void WddmMemoryManager::unmapPhysicalHostMemoryFromCpuAddress(WddmAllocation &mappedAllocation) {
}

bool WddmMemoryManager::isHostMemoryMappable(const void *ptr, size_t size, bool writable) {
    return true;
}
} // namespace NEO

/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {

MockGraphicsAllocation::MockGraphicsAllocation() : MemoryAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0u, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu) {}

MockGraphicsAllocation::MockGraphicsAllocation(void *buffer, size_t sizeIn) : MemoryAllocation(0, 1u /*num gmms*/, AllocationType::unknown, buffer, castToUint64(buffer), 0llu, sizeIn, MemoryPool::memoryNull, MemoryManager::maxOsContextCount) {}

MockGraphicsAllocation::MockGraphicsAllocation(void *buffer, uint64_t gpuAddr, size_t sizeIn) : MemoryAllocation(0, 1u /*num gmms*/, AllocationType::unknown, buffer, gpuAddr, 0llu, sizeIn, MemoryPool::memoryNull, MemoryManager::maxOsContextCount) {}

MockGraphicsAllocation::MockGraphicsAllocation(uint32_t rootDeviceIndex, void *buffer, size_t sizeIn) : MemoryAllocation(rootDeviceIndex, 1u /*num gmms*/, AllocationType::unknown, buffer, castToUint64(buffer), 0llu, sizeIn, MemoryPool::memoryNull, MemoryManager::maxOsContextCount) {}

} // namespace NEO

/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/device_bitfield.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>

namespace NEO {

class MemoryManager;
class CommandStreamReceiver;

struct HostFunctionChunk {
    GraphicsAllocation *allocation = nullptr;
    uint8_t *cpuPtr = nullptr;
};

class HostFunctionAllocator : public NonCopyableAndNonMovableClass {
  public:
    static constexpr size_t partitionOffset = 32u;
    static constexpr size_t chunkAlignment = 256u;

    HostFunctionAllocator(MemoryManager *memoryManager,
                          CommandStreamReceiver *csr,
                          size_t storageSize,
                          uint32_t rootDeviceIndex,
                          uint32_t maxRootDeviceIndex,
                          uint32_t activePartitions,
                          bool isTbx);

    MOCKABLE_VIRTUAL ~HostFunctionAllocator();
    MOCKABLE_VIRTUAL HostFunctionChunk obtainChunk();

  protected:
    void allocateStorage();

    MemoryManager *memoryManager = nullptr;
    CommandStreamReceiver *csr = nullptr;
    std::unique_ptr<MultiGraphicsAllocation> storage = nullptr;
    size_t currentOffset = 0u;
    size_t storageSize = 0u;
    uint32_t rootDeviceIndex = 0u;
    uint32_t maxRootDeviceIndex = 0u;
    uint32_t activePartitions = 0u;
    bool isTbx = false;
    std::mutex allocationMutex;
};

static_assert(NonCopyableAndNonMovable<HostFunctionAllocator>);

} // namespace NEO

/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/device_bitfield.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/source/utilities/spinlock.h"

#include <level_zero/ze_api.h>

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

namespace NEO {
class GraphicsAllocation;
class MemoryManager;
} // namespace NEO

namespace L0 {
struct Device;

struct HostPointerData {
    HostPointerData(uint32_t maxRootDeviceIndex)
        : hostPtrAllocations(maxRootDeviceIndex),
          maxRootDeviceIndex(maxRootDeviceIndex) {
    }
    HostPointerData(const HostPointerData &hostPtrData)
        : HostPointerData(hostPtrData.maxRootDeviceIndex) {
        basePtr = hostPtrData.basePtr;
        size = hostPtrData.size;
        for (auto allocation : hostPtrData.hostPtrAllocations.getGraphicsAllocations()) {
            if (allocation) {
                this->hostPtrAllocations.addAllocation(allocation);
            }
        }
    }
    HostPointerData(HostPointerData &&other) noexcept = delete;
    HostPointerData &operator=(HostPointerData &&other) noexcept = delete;
    HostPointerData &operator=(const HostPointerData &other) = delete;

    NEO::MultiGraphicsAllocation hostPtrAllocations;
    void *basePtr = nullptr;
    size_t size = 0u;

  protected:
    const uint32_t maxRootDeviceIndex;
};

class HostPointerManager {
  public:
    class MapBasedAllocationTracker {
        friend class HostPointerManager;

      public:
        using HostPointerContainer = std::map<const void *, HostPointerData>;
        void insert(const HostPointerData &allocationsData);
        void remove(const void *ptr);
        HostPointerData *get(const void *ptr);
        size_t getNumAllocs() const { return allocations.size(); };

      protected:
        HostPointerContainer allocations;
    };

    HostPointerManager(NEO::MemoryManager *memoryManager);
    virtual ~HostPointerManager();
    ze_result_t createHostPointerMultiAllocation(std::vector<Device *> &devices, void *ptr, size_t size);
    HostPointerData *getHostPointerAllocation(const void *ptr);
    bool freeHostPointerAllocation(void *ptr);

  protected:
    NEO::GraphicsAllocation *createHostPointerAllocation(uint32_t rootDeviceIndex,
                                                         void *ptr,
                                                         size_t size,
                                                         const NEO::DeviceBitfield &deviceBitfield);

    MapBasedAllocationTracker hostPointerAllocations;
    NEO::MemoryManager *memoryManager;
    NEO::SpinLock mtx;
};

static_assert(NEO::NonMovable<HostPointerData>);

} // namespace L0

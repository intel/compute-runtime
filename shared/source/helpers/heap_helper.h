/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <cstdint>

namespace NEO {

class MemoryManager;
class GraphicsAllocation;
class InternalAllocationStorage;
class Device;

class HeapHelper {
  public:
    HeapHelper(MemoryManager *memManager, InternalAllocationStorage *storageForReuse, bool isMultiOsContextCapable) : isMultiOsContextCapable(isMultiOsContextCapable),
                                                                                                                      storageForReuse(storageForReuse),
                                                                                                                      memManager(memManager) {}
    HeapHelper(Device *device, InternalAllocationStorage *storageForReuse, bool isMultiOsContextCapable);
    GraphicsAllocation *getHeapAllocation(uint32_t heapType, size_t heapSize, size_t alignment, uint32_t rootDeviceIndex);
    void storeHeapAllocation(GraphicsAllocation *heapAllocation);
    bool isMultiOsContextCapable = false;

  protected:
    InternalAllocationStorage *storageForReuse = nullptr;
    MemoryManager *memManager = nullptr;
    Device *device = nullptr;
};
} // namespace NEO

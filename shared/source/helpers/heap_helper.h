/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <stddef.h>
#include <stdint.h>

namespace NEO {

class MemoryManager;
class GraphicsAllocation;
class InternalAllocationStorage;

class HeapHelper {
  public:
    HeapHelper(MemoryManager *memManager, InternalAllocationStorage *storageForReuse, bool isMultiOsContextCapable) : isMultiOsContextCapable(isMultiOsContextCapable),
                                                                                                                      storageForReuse(storageForReuse),
                                                                                                                      memManager(memManager) {}
    GraphicsAllocation *getHeapAllocation(uint32_t heapType, size_t heapSize, size_t alignment, uint32_t rootDeviceIndex);
    void storeHeapAllocation(GraphicsAllocation *heapAllocation);
    bool isMultiOsContextCapable = false;

  protected:
    InternalAllocationStorage *storageForReuse = nullptr;
    MemoryManager *memManager = nullptr;
};
} // namespace NEO

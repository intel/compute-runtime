/*
 * Copyright (C) 2019 Intel Corporation
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
    HeapHelper(MemoryManager *memManager, InternalAllocationStorage *storageForReuse, bool isMultiOsContextCapable) : storageForReuse(storageForReuse),
                                                                                                                      memManager(memManager),
                                                                                                                      isMultiOsContextCapable(isMultiOsContextCapable) {}
    GraphicsAllocation *getHeapAllocation(size_t heapSize, size_t alignment, uint32_t rootDeviceIndex);
    void storeHeapAllocation(GraphicsAllocation *heapAllocation);

  protected:
    InternalAllocationStorage *storageForReuse = nullptr;
    MemoryManager *memManager = nullptr;
    bool isMultiOsContextCapable = false;
};
} // namespace NEO

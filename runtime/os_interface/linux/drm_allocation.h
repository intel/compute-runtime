/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/graphics_allocation.h"

namespace NEO {
class BufferObject;

struct OsHandle {
    BufferObject *bo = nullptr;
};

class DrmAllocation : public GraphicsAllocation {
  public:
    DrmAllocation(AllocationType allocationType, BufferObject *bo, void *ptrIn, size_t sizeIn, osHandle sharedHandle, MemoryPool::Type pool, bool multiOsContextCapable)
        : GraphicsAllocation(allocationType, ptrIn, sizeIn, sharedHandle, pool, multiOsContextCapable),
          bo(bo) {
    }

    DrmAllocation(AllocationType allocationType, BufferObject *bo, void *ptrIn, uint64_t gpuAddress, size_t sizeIn, MemoryPool::Type pool, bool multiOsContextCapable)
        : GraphicsAllocation(allocationType, ptrIn, gpuAddress, 0, sizeIn, pool, multiOsContextCapable),
          bo(bo) {
    }

    std::string getAllocationInfoString() const override;

    BufferObject *getBO() const {
        if (fragmentsStorage.fragmentCount) {
            return fragmentsStorage.fragmentStorageData[0].osHandleStorage->bo;
        }
        return this->bo;
    }
    uint64_t peekInternalHandle(MemoryManager *memoryManager) override;

  protected:
    BufferObject *bo;
};
} // namespace NEO

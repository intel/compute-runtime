/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/graphics_allocation.h"

namespace OCLRT {
class BufferObject;

struct OsHandle {
    BufferObject *bo = nullptr;
};

class DrmAllocation : public GraphicsAllocation {
  public:
    DrmAllocation(BufferObject *bo, void *ptrIn, size_t sizeIn, MemoryPool::Type pool, bool multiOsContextCapable) : GraphicsAllocation(ptrIn, castToUint64(ptrIn), 0llu, sizeIn, multiOsContextCapable), bo(bo) {
        this->memoryPool = pool;
    }
    DrmAllocation(BufferObject *bo, void *ptrIn, size_t sizeIn, osHandle sharedHandle, MemoryPool::Type pool, bool multiOsContextCapable) : GraphicsAllocation(ptrIn, sizeIn, sharedHandle, multiOsContextCapable), bo(bo) {
        this->memoryPool = pool;
    }
    DrmAllocation(BufferObject *bo, void *ptrIn, uint64_t gpuAddress, size_t sizeIn, MemoryPool::Type pool, bool multiOsContextCapable) : GraphicsAllocation(ptrIn, gpuAddress, 0, sizeIn, multiOsContextCapable), bo(bo) {
        this->memoryPool = pool;
    }

    BufferObject *getBO() const {
        if (fragmentsStorage.fragmentCount) {
            return fragmentsStorage.fragmentStorageData[0].osHandleStorage->bo;
        }
        return this->bo;
    }

  protected:
    BufferObject *bo;
};
} // namespace OCLRT

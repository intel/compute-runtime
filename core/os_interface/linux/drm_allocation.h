/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/graphics_allocation.h"

namespace NEO {
class BufferObject;

struct OsHandle {
    BufferObject *bo = nullptr;
};

using BufferObjects = std::array<BufferObject *, EngineLimits::maxHandleCount>;

class DrmAllocation : public GraphicsAllocation {
  public:
    DrmAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, BufferObject *bo, void *ptrIn, size_t sizeIn, osHandle sharedHandle, MemoryPool::Type pool)
        : GraphicsAllocation(rootDeviceIndex, allocationType, ptrIn, sizeIn, sharedHandle, pool),
          bufferObjects({{bo}}) {
    }

    DrmAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, BufferObject *bo, void *ptrIn, uint64_t gpuAddress, size_t sizeIn, MemoryPool::Type pool)
        : GraphicsAllocation(rootDeviceIndex, allocationType, ptrIn, gpuAddress, 0, sizeIn, pool),
          bufferObjects({{bo}}) {
    }

    DrmAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, BufferObjects &bos, void *ptrIn, uint64_t gpuAddress, size_t sizeIn, MemoryPool::Type pool)
        : GraphicsAllocation(rootDeviceIndex, allocationType, ptrIn, gpuAddress, 0, sizeIn, pool),
          bufferObjects(bos) {
    }

    std::string getAllocationInfoString() const override;

    BufferObject *getBO() const {
        if (fragmentsStorage.fragmentCount) {
            return fragmentsStorage.fragmentStorageData[0].osHandleStorage->bo;
        }
        return this->bufferObjects[0];
    }
    const BufferObjects &getBOs() const {
        return this->bufferObjects;
    }
    BufferObject *&getBufferObjectToModify(uint32_t handleIndex) {
        return bufferObjects[handleIndex];
    }

    uint64_t peekInternalHandle(MemoryManager *memoryManager) override;

  protected:
    BufferObjects bufferObjects{};
};
} // namespace NEO

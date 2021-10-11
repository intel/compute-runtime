/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memadvise_flags.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {
class BufferObject;
class OsContext;
class Drm;
enum class CachePolicy : uint32_t;
enum class CacheRegion : uint16_t;

struct OsHandleLinux : OsHandle {
    BufferObject *bo = nullptr;
};

using BufferObjects = StackVec<BufferObject *, EngineLimits::maxHandleCount>;

class DrmAllocation : public GraphicsAllocation {
  public:
    DrmAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, BufferObject *bo, void *ptrIn, size_t sizeIn, osHandle sharedHandle, MemoryPool::Type pool)
        : DrmAllocation(rootDeviceIndex, 1, allocationType, bo, ptrIn, sizeIn, sharedHandle, pool) {}

    DrmAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, BufferObject *bo, void *ptrIn, size_t sizeIn, osHandle sharedHandle, MemoryPool::Type pool)
        : GraphicsAllocation(rootDeviceIndex, numGmms, allocationType, ptrIn, sizeIn, sharedHandle, pool, MemoryManager::maxOsContextCount), bufferObjects(EngineLimits::maxHandleCount) {
        bufferObjects[0] = bo;
    }

    DrmAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, BufferObject *bo, void *ptrIn, uint64_t gpuAddress, size_t sizeIn, MemoryPool::Type pool)
        : DrmAllocation(rootDeviceIndex, 1, allocationType, bo, ptrIn, gpuAddress, sizeIn, pool) {}

    DrmAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, BufferObject *bo, void *ptrIn, uint64_t gpuAddress, size_t sizeIn, MemoryPool::Type pool)
        : GraphicsAllocation(rootDeviceIndex, numGmms, allocationType, ptrIn, gpuAddress, 0, sizeIn, pool, MemoryManager::maxOsContextCount), bufferObjects(EngineLimits::maxHandleCount) {
        bufferObjects[0] = bo;
    }

    DrmAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, BufferObjects &bos, void *ptrIn, uint64_t gpuAddress, size_t sizeIn, MemoryPool::Type pool)
        : DrmAllocation(rootDeviceIndex, 1, allocationType, bos, ptrIn, gpuAddress, sizeIn, pool) {}

    DrmAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, BufferObjects &bos, void *ptrIn, uint64_t gpuAddress, size_t sizeIn, MemoryPool::Type pool)
        : GraphicsAllocation(rootDeviceIndex, numGmms, allocationType, ptrIn, gpuAddress, 0, sizeIn, pool, MemoryManager::maxOsContextCount),
          bufferObjects(bos) {
    }

    std::string getAllocationInfoString() const override;

    BufferObject *getBO() const {
        if (fragmentsStorage.fragmentCount) {
            return static_cast<OsHandleLinux *>(fragmentsStorage.fragmentStorageData[0].osHandleStorage)->bo;
        }
        return this->bufferObjects[0];
    }
    const BufferObjects &getBOs() const {
        return this->bufferObjects;
    }
    BufferObject *&getBufferObjectToModify(uint32_t handleIndex) {
        return bufferObjects[handleIndex];
    }

    void resizeBufferObjects(uint32_t size) {
        this->bufferObjects.resize(size);
    }

    uint64_t peekInternalHandle(MemoryManager *memoryManager) override;

    bool setCacheRegion(Drm *drm, CacheRegion regionIndex);
    bool setCacheAdvice(Drm *drm, size_t regionSize, CacheRegion regionIndex);
    void setCachePolicy(CachePolicy memType);

    bool setMemAdvise(Drm *drm, MemAdviseFlags flags);

    void *getMmapPtr() { return this->mmapPtr; }
    void setMmapPtr(void *ptr) { this->mmapPtr = ptr; }
    size_t getMmapSize() { return this->mmapSize; }
    void setMmapSize(size_t size) { this->mmapSize = size; }

    void makeBOsResident(OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind);
    void bindBO(BufferObject *bo, OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind);
    void bindBOs(OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind);
    MOCKABLE_VIRTUAL void registerBOBindExtHandle(Drm *drm);
    void freeRegisteredBOBindExtHandles(Drm *drm);
    void linkWithRegisteredHandle(uint32_t handle);
    MOCKABLE_VIRTUAL void markForCapture();

  protected:
    BufferObjects bufferObjects{};
    StackVec<uint32_t, 1> registeredBoBindHandles;
    MemAdviseFlags enabledMemAdviseFlags{};

    void *mmapPtr = nullptr;
    size_t mmapSize = 0u;
};
} // namespace NEO

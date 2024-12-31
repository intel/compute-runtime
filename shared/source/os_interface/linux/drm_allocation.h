/*
 * Copyright (C) 2018-2025 Intel Corporation
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
class OsContextLinux;
enum class CachePolicy : uint32_t;
enum class CacheRegion : uint16_t;
enum class AtomicAccessMode : uint32_t;

struct OsHandleLinux : OsHandle {
    ~OsHandleLinux() override = default;
    BufferObject *bo = nullptr;
};

using BufferObjects = StackVec<BufferObject *, EngineLimits::maxHandleCount>;

class DrmAllocation : public GraphicsAllocation {
  public:
    using MemoryUnmapFunction = int (*)(void *, size_t);

    struct MemoryToUnmap {
        void *pointer;
        size_t size;
        MemoryUnmapFunction unmapFunction;
    };

    DrmAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, BufferObject *bo, void *ptrIn, size_t sizeIn, osHandle sharedHandle, MemoryPool pool, uint64_t canonizedGpuAddress)
        : GraphicsAllocation(rootDeviceIndex, numGmms, allocationType, ptrIn, sizeIn, sharedHandle, pool, MemoryManager::maxOsContextCount, canonizedGpuAddress) {
        bufferObjects.push_back(bo);
        resizeBufferObjects(EngineLimits::maxHandleCount);
        handles.resize(EngineLimits::maxHandleCount, std::numeric_limits<uint64_t>::max());
    }

    DrmAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, BufferObject *bo, void *ptrIn, uint64_t canonizedGpuAddress, size_t sizeIn, MemoryPool pool)
        : GraphicsAllocation(rootDeviceIndex, numGmms, allocationType, ptrIn, canonizedGpuAddress, 0, sizeIn, pool, MemoryManager::maxOsContextCount) {
        bufferObjects.push_back(bo);
        resizeBufferObjects(EngineLimits::maxHandleCount);
        handles.resize(EngineLimits::maxHandleCount, std::numeric_limits<uint64_t>::max());
    }

    DrmAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, BufferObjects &bos, void *ptrIn, uint64_t canonizedGpuAddress, size_t sizeIn, MemoryPool pool)
        : GraphicsAllocation(rootDeviceIndex, numGmms, allocationType, ptrIn, canonizedGpuAddress, 0, sizeIn, pool, MemoryManager::maxOsContextCount),
          bufferObjects(bos) {
        handles.resize(EngineLimits::maxHandleCount, std::numeric_limits<uint64_t>::max());
    }

    ~DrmAllocation() override;

    std::string getAllocationInfoString() const override;
    std::string getPatIndexInfoString(const ProductHelper &productHelper) const override;

    BufferObject *getBO() const {
        if (fragmentsStorage.fragmentCount) {
            return static_cast<OsHandleLinux *>(fragmentsStorage.fragmentStorageData[0].osHandleStorage)->bo;
        }
        return this->bufferObjects[0];
    }
    const BufferObjects &getBOs() const {
        return this->bufferObjects;
    }
    MOCKABLE_VIRTUAL BufferObject *&getBufferObjectToModify(uint32_t handleIndex) {
        return bufferObjects[handleIndex];
    }

    void resizeBufferObjects(uint32_t size) {
        this->bufferObjects.resize(size);
    }

    uint32_t getNumHandles() override {
        return this->numHandles;
    }

    void setNumHandles(uint32_t numHandles) override {
        this->numHandles = numHandles;
    }

    uint64_t getHandleAddressBase(uint32_t handleIndex) override;

    size_t getHandleSize(uint32_t handleIndex) override;
    int createInternalHandle(MemoryManager *memoryManager, uint32_t handleId, uint64_t &handle) override;

    void clearInternalHandle(uint32_t handleId) override;

    int peekInternalHandle(MemoryManager *memoryManager, uint64_t &handle) override;

    int peekInternalHandle(MemoryManager *memoryManager, uint32_t handleId, uint64_t &handle) override;

    bool setCacheRegion(Drm *drm, CacheRegion regionIndex);
    bool setCacheAdvice(Drm *drm, size_t regionSize, CacheRegion regionIndex, bool isSystemMemoryPool);
    void setCachePolicy(CachePolicy memType);

    bool setPreferredLocation(Drm *drm, PreferredLocation memoryLocation);

    bool setMemAdvise(Drm *drm, MemAdviseFlags flags);
    bool setAtomicAccess(Drm *drm, size_t size, AtomicAccessMode mode);
    bool setMemPrefetch(Drm *drm, SubDeviceIdsVec &subDeviceIds);

    void *getMmapPtr() {
        if (this->importedMmapPtr)
            return this->importedMmapPtr;
        else
            return this->mmapPtr;
    }
    void setMmapPtr(void *ptr) { this->mmapPtr = ptr; }
    void setImportedMmapPtr(void *ptr) { this->importedMmapPtr = ptr; }
    size_t getMmapSize() { return this->mmapSize; }
    void setMmapSize(size_t size) { this->mmapSize = size; }

    void setUsmHostAllocation(bool flag) { usmHostAllocation = flag; }
    bool isUsmHostAllocation() { return usmHostAllocation; }

    OsContextLinux *getOsContext() const {
        return this->osContext;
    }

    void setOsContext(OsContextLinux *context) {
        this->osContext = context;
    }

    bool prefetchBOWithChunking(Drm *drm);
    MOCKABLE_VIRTUAL int makeBOsResident(OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind, const bool forcePagingFence);
    MOCKABLE_VIRTUAL int bindBO(BufferObject *bo, OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind, const bool forcePagingFence);
    MOCKABLE_VIRTUAL int bindBOs(OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind, const bool forcePagingFence);
    MOCKABLE_VIRTUAL bool prefetchBO(BufferObject *bo, uint32_t vmHandleId, uint32_t subDeviceId);
    MOCKABLE_VIRTUAL void registerBOBindExtHandle(Drm *drm);
    void addRegisteredBoBindHandle(uint32_t handle) { registeredBoBindHandles.push_back(handle); }
    void freeRegisteredBOBindExtHandles(Drm *drm);
    void linkWithRegisteredHandle(uint32_t handle);
    MOCKABLE_VIRTUAL void markForCapture();
    MOCKABLE_VIRTUAL bool shouldAllocationPageFault(const Drm *drm);
    void registerMemoryToUnmap(void *pointer, size_t size, MemoryUnmapFunction unmapFunction);
    void setAsReadOnly() override;

  protected:
    OsContextLinux *osContext = nullptr;
    BufferObjects bufferObjects{};
    StackVec<uint32_t, 1> registeredBoBindHandles;
    StackVec<MemoryToUnmap, 1> memoryToUnmap;
    std::vector<uint64_t> handles;

    void *mmapPtr = nullptr;
    void *importedMmapPtr = nullptr;
    size_t mmapSize = 0u;
    uint32_t numHandles = 0u;
    MemAdviseFlags enabledMemAdviseFlags{};

    bool usmHostAllocation = false;
};
} // namespace NEO

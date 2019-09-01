/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <atomic>
#include <cstddef>
#include <stdint.h>

struct drm_i915_gem_exec_object2;
struct drm_i915_gem_relocation_entry;

namespace NEO {

class DrmMemoryManager;
class Drm;

class BufferObject {
    friend DrmMemoryManager;

  public:
    BufferObject(Drm *drm, int handle);
    BufferObject(Drm *drm, int handle, size_t size);
    MOCKABLE_VIRTUAL ~BufferObject(){};

    bool setTiling(uint32_t mode, uint32_t stride);

    MOCKABLE_VIRTUAL int pin(BufferObject *const boToPin[], size_t numberOfBos, uint32_t drmContextId);

    int exec(uint32_t used, size_t startOffset, unsigned int flags, bool requiresCoherency, uint32_t drmContextId, BufferObject *const residency[], size_t residencyCount, drm_i915_gem_exec_object2 *execObjectsStorage);

    int wait(int64_t timeoutNs);
    bool close();

    inline void reference() {
        this->refCount++;
    }
    uint32_t getRefCount() const;

    size_t peekSize() const { return size; }
    int peekHandle() const { return handle; }
    uint64_t peekAddress() const { return gpuAddress; }
    void setAddress(uint64_t address) { this->gpuAddress = address; }
    void *peekLockedAddress() const { return lockedAddress; }
    void setLockedAddress(void *cpuAddress) { this->lockedAddress = cpuAddress; }
    void setUnmapSize(uint64_t unmapSize) { this->unmapSize = unmapSize; }
    uint64_t peekUnmapSize() const { return unmapSize; }
    bool peekIsReusableAllocation() const { return this->isReused; }

  protected:
    Drm *drm;

    std::atomic<uint32_t> refCount;

    int handle; // i915 gem object handle
    bool isReused;

    //Tiling
    uint32_t tiling_mode;

    MOCKABLE_VIRTUAL void fillExecObject(drm_i915_gem_exec_object2 &execObject, uint32_t drmContextId);

    uint64_t gpuAddress = 0llu;

    uint64_t size;
    void *lockedAddress; // CPU side virtual address

    uint64_t unmapSize = 0;
};
} // namespace NEO

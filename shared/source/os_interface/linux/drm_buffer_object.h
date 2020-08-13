/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "drm/i915_drm.h"
#include "engine_limits.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <stdint.h>
#include <vector>

struct drm_i915_gem_exec_object2;
struct drm_i915_gem_relocation_entry;

namespace NEO {

class DrmMemoryManager;
class Drm;
class OsContext;

class BufferObject {
    friend DrmMemoryManager;

  public:
    BufferObject(Drm *drm, int handle, size_t size, size_t maxOsContextCount);
    MOCKABLE_VIRTUAL ~BufferObject(){};

    struct Deleter {
        void operator()(BufferObject *bo) {
            bo->close();
            delete bo;
        }
    };

    bool setTiling(uint32_t mode, uint32_t stride);

    MOCKABLE_VIRTUAL int pin(BufferObject *const boToPin[], size_t numberOfBos, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId);

    int exec(uint32_t used, size_t startOffset, unsigned int flags, bool requiresCoherency, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId, BufferObject *const residency[], size_t residencyCount, drm_i915_gem_exec_object2 *execObjectsStorage);

    void bind(OsContext *osContext, uint32_t vmHandleId);
    void unbind(OsContext *osContext, uint32_t vmHandleId);

    void printExecutionBuffer(drm_i915_gem_execbuffer2 &execbuf, const size_t &residencyCount, drm_i915_gem_exec_object2 *execObjectsStorage, BufferObject *const residency[]);

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
    Drm *drm = nullptr;
    bool perContextVmsUsed = false;
    std::atomic<uint32_t> refCount;

    int handle; // i915 gem object handle
    uint64_t size;
    bool isReused;

    //Tiling
    uint32_t tiling_mode;

    MOCKABLE_VIRTUAL void fillExecObject(drm_i915_gem_exec_object2 &execObject, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId);
    void fillExecObjectImpl(drm_i915_gem_exec_object2 &execObject, OsContext *osContext, uint32_t vmHandleId);

    uint64_t gpuAddress = 0llu;

    void *lockedAddress; // CPU side virtual address

    uint64_t unmapSize = 0;

    std::vector<std::array<bool, EngineLimits::maxHandleCount>> bindInfo;
};
} // namespace NEO

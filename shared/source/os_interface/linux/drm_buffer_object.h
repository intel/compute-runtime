/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/memory_manager/definitions/engine_limits.h"
#include "shared/source/os_interface/linux/cache_info.h"
#include "shared/source/utilities/stackvec.h"

#include "drm/i915_drm.h"

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
  public:
    BufferObject(Drm *drm, int handle, size_t size, size_t maxOsContextCount);
    MOCKABLE_VIRTUAL ~BufferObject() = default;

    struct Deleter {
        void operator()(BufferObject *bo) {
            bo->close();
            delete bo;
        }
    };

    bool setTiling(uint32_t mode, uint32_t stride);

    int pin(BufferObject *const boToPin[], size_t numberOfBos, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId);
    MOCKABLE_VIRTUAL int validateHostPtr(BufferObject *const boToPin[], size_t numberOfBos, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId);

    int exec(uint32_t used, size_t startOffset, unsigned int flags, bool requiresCoherency, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId, BufferObject *const residency[], size_t residencyCount, drm_i915_gem_exec_object2 *execObjectsStorage);

    int bind(OsContext *osContext, uint32_t vmHandleId);
    int unbind(OsContext *osContext, uint32_t vmHandleId);

    void printExecutionBuffer(drm_i915_gem_execbuffer2 &execbuf, const size_t &residencyCount, drm_i915_gem_exec_object2 *execObjectsStorage, BufferObject *const residency[]);

    int wait(int64_t timeoutNs);
    bool close();

    inline void reference() {
        this->refCount++;
    }
    inline uint32_t unreference() {
        return this->refCount.fetch_sub(1);
    }
    uint32_t getRefCount() const;

    size_t peekSize() const { return size; }
    int peekHandle() const { return handle; }
    const Drm *peekDrm() const { return drm; }
    uint64_t peekAddress() const { return gpuAddress; }
    void setAddress(uint64_t address) { this->gpuAddress = GmmHelper::canonize(address); }
    void *peekLockedAddress() const { return lockedAddress; }
    void setLockedAddress(void *cpuAddress) { this->lockedAddress = cpuAddress; }
    void setUnmapSize(uint64_t unmapSize) { this->unmapSize = unmapSize; }
    uint64_t peekUnmapSize() const { return unmapSize; }
    bool peekIsReusableAllocation() const { return this->isReused; }
    void markAsReusableAllocation() { this->isReused = true; }
    void addBindExtHandle(uint32_t handle);
    StackVec<uint32_t, 2> &getBindExtHandles() { return bindExtHandles; }
    void markForCapture() {
        allowCapture = true;
    }
    bool isMarkedForCapture() {
        return allowCapture;
    }

    bool isImmediateBindingRequired() {
        return requiresImmediateBinding;
    }
    void requireImmediateBinding(bool required) {
        requiresImmediateBinding = required;
    }

    void setCacheRegion(CacheRegion regionIndex) { cacheRegion = regionIndex; }
    CacheRegion peekCacheRegion() const { return cacheRegion; }

    void setCachePolicy(CachePolicy memType) { cachePolicy = memType; }
    CachePolicy peekCachePolicy() const { return cachePolicy; }

    void setColourWithBind() {
        this->colourWithBind = true;
    }
    void setColourChunk(size_t size) {
        this->colourChunk = size;
    }
    void addColouringAddress(uint64_t address) {
        this->bindAddresses.push_back(address);
    }
    void reserveAddressVector(size_t size) {
        this->bindAddresses.reserve(size);
    }

    bool getColourWithBind() {
        return this->colourWithBind;
    }
    size_t getColourChunk() {
        return this->colourChunk;
    }
    std::vector<uint64_t> &getColourAddresses() {
        return this->bindAddresses;
    }

  protected:
    Drm *drm = nullptr;
    bool perContextVmsUsed = false;
    std::atomic<uint32_t> refCount;

    int handle; // i915 gem object handle
    uint64_t size;
    bool isReused;

    //Tiling
    uint32_t tiling_mode;
    bool allowCapture = false;
    bool requiresImmediateBinding = false;

    uint32_t getOsContextId(OsContext *osContext);
    MOCKABLE_VIRTUAL void fillExecObject(drm_i915_gem_exec_object2 &execObject, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId);
    void fillExecObjectImpl(drm_i915_gem_exec_object2 &execObject, OsContext *osContext, uint32_t vmHandleId);

    void *lockedAddress; // CPU side virtual address

    uint64_t unmapSize = 0;

    CacheRegion cacheRegion = CacheRegion::Default;
    CachePolicy cachePolicy = CachePolicy::WriteBack;

    std::vector<std::array<bool, EngineLimits::maxHandleCount>> bindInfo;
    StackVec<uint32_t, 2> bindExtHandles;

    bool colourWithBind = false;
    size_t colourChunk = 0;
    std::vector<uint64_t> bindAddresses;

  private:
    uint64_t gpuAddress = 0llu;
};
} // namespace NEO

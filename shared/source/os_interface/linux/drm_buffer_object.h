/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "drm/i915_drm.h"

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>

namespace NEO {

class DrmMemoryManager;
class Drm;

class BufferObject {
    friend DrmMemoryManager;

  public:
    BufferObject(Drm *drm, int handle, size_t size);
    MOCKABLE_VIRTUAL ~BufferObject(){};

    bool setTiling(uint32_t mode, uint32_t stride);

    MOCKABLE_VIRTUAL int pin(BufferObject *const boToPin[], size_t numberOfBos, uint32_t drmContextId);

    template <typename Iterator>
    int exec(uint32_t used, size_t startOffset, unsigned int flags, bool requiresCoherency, uint32_t drmContextId, Iterator begin, size_t residencyCount, drm_i915_gem_exec_object2 *execObjectsStorage);

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

  protected:
    Drm *drm = nullptr;

    std::atomic<uint32_t> refCount;

    int handle; // i915 gem object handle
    uint64_t size;
    bool isReused;

    //Tiling
    uint32_t tiling_mode;

    MOCKABLE_VIRTUAL void fillExecObject(drm_i915_gem_exec_object2 &execObject, uint32_t drmContextId);

    uint64_t gpuAddress = 0llu;

    void *lockedAddress; // CPU side virtual address

    uint64_t unmapSize = 0;
};

template <typename Iterator>
inline int BufferObject::exec(uint32_t used, size_t startOffset, unsigned int flags, bool requiresCoherency, uint32_t drmContextId, Iterator begin, size_t residencyCount, drm_i915_gem_exec_object2 *execObjectsStorage) {
    for (size_t i = 0; i < residencyCount; ++i) {
        (*begin++)->fillExecObject(execObjectsStorage[i], drmContextId);
    }
    this->fillExecObject(execObjectsStorage[residencyCount], drmContextId);

    drm_i915_gem_execbuffer2 execbuf{};
    execbuf.buffers_ptr = reinterpret_cast<uintptr_t>(execObjectsStorage);
    execbuf.buffer_count = static_cast<uint32_t>(residencyCount + 1u);
    execbuf.batch_start_offset = static_cast<uint32_t>(startOffset);
    execbuf.batch_len = alignUp(used, 8);
    execbuf.flags = flags;
    execbuf.rsvd1 = drmContextId;

    int ret = this->drm->ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2, &execbuf);
    if (ret == 0) {
        return 0;
    }

    int err = this->drm->getErrno();
    printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(I915_GEM_EXECBUFFER2) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
    return err;
}
} // namespace NEO

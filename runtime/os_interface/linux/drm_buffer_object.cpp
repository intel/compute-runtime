/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_buffer_object.h"

#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "runtime/os_interface/linux/drm_neo.h"
#include "runtime/os_interface/linux/os_time_linux.h"
#include "runtime/utilities/stackvec.h"

#include "drm/i915_drm.h"

#include <errno.h>
#include <fcntl.h>
#include <map>
#include <stdarg.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

namespace NEO {

BufferObject::BufferObject(Drm *drm, int handle, bool isAllocated) : drm(drm), refCount(1), handle(handle), isReused(false), isAllocated(isAllocated) {
    this->tiling_mode = I915_TILING_NONE;
    this->stride = 0;
    this->size = 0;
    this->lockedAddress = nullptr;
}

uint32_t BufferObject::getRefCount() const {
    return this->refCount.load();
}

bool BufferObject::close() {
    drm_gem_close close = {};
    close.handle = this->handle;

    int ret = this->drm->ioctl(DRM_IOCTL_GEM_CLOSE, &close);
    if (ret != 0) {
        int err = errno;
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(GEM_CLOSE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        DEBUG_BREAK_IF(true);
        return false;
    }

    this->handle = -1;

    return true;
}

int BufferObject::wait(int64_t timeoutNs) {
    drm_i915_gem_wait wait = {};
    wait.bo_handle = this->handle;
    wait.timeout_ns = -1;

    int ret = this->drm->ioctl(DRM_IOCTL_I915_GEM_WAIT, &wait);
    if (ret != 0) {
        int err = errno;
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(I915_GEM_WAIT) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
    }
    UNRECOVERABLE_IF(ret != 0);

    return ret;
}

bool BufferObject::setTiling(uint32_t mode, uint32_t stride) {
    if (this->tiling_mode == mode) {
        return true;
    }

    drm_i915_gem_set_tiling set_tiling = {};
    set_tiling.handle = this->handle;
    set_tiling.tiling_mode = mode;
    set_tiling.stride = stride;

    if (this->drm->ioctl(DRM_IOCTL_I915_GEM_SET_TILING, &set_tiling) != 0) {
        return false;
    }

    this->tiling_mode = set_tiling.tiling_mode;
    this->stride = set_tiling.stride;

    return set_tiling.tiling_mode == mode;
}

void BufferObject::fillExecObject(drm_i915_gem_exec_object2 &execObject, uint32_t drmContextId) {
    execObject.handle = this->handle;
    execObject.relocation_count = 0; //No relocations, we are SoftPinning
    execObject.relocs_ptr = 0ul;
    execObject.alignment = 0;
    execObject.offset = this->gpuAddress;
    execObject.flags = EXEC_OBJECT_PINNED;
#ifdef __x86_64__
    // set EXEC_OBJECT_SUPPORTS_48B_ADDRESS flag if whole object resides in 32BIT address space boundary
    execObject.flags |= (this->gpuAddress + this->size) & MemoryConstants::zoneHigh ? EXEC_OBJECT_SUPPORTS_48B_ADDRESS : 0;
#endif
    execObject.rsvd1 = drmContextId;
    execObject.rsvd2 = 0;
}

void BufferObject::processRelocs(int &idx, uint32_t drmContextId, ResidencyVector &residency, drm_i915_gem_exec_object2 *execObjectsStorage) {
    for (size_t i = 0; i < residency.size(); i++) {
        residency[i]->fillExecObject(execObjectsStorage[idx], drmContextId);
        idx++;
    }
}

int BufferObject::exec(uint32_t used, size_t startOffset, unsigned int flags, bool requiresCoherency, uint32_t drmContextId, ResidencyVector &residency, drm_i915_gem_exec_object2 *execObjectsStorage) {
    drm_i915_gem_execbuffer2 execbuf = {};

    int idx = 0;
    processRelocs(idx, drmContextId, residency, execObjectsStorage);
    this->fillExecObject(execObjectsStorage[idx], drmContextId);
    idx++;

    execbuf.buffers_ptr = reinterpret_cast<uintptr_t>(execObjectsStorage);
    execbuf.buffer_count = idx;
    execbuf.batch_start_offset = static_cast<uint32_t>(startOffset);
    execbuf.batch_len = alignUp(used, 8);
    execbuf.flags = flags;
    execbuf.rsvd1 = drmContextId;

    int ret = this->drm->ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2, &execbuf);
    if (ret != 0) {
        int err = errno;
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(I915_GEM_EXECBUFFER2) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        UNRECOVERABLE_IF(true);
    }

    return ret;
}

int BufferObject::pin(BufferObject *const boToPin[], size_t numberOfBos, uint32_t drmContextId) {
    drm_i915_gem_execbuffer2 execbuf = {};
    StackVec<drm_i915_gem_exec_object2, maxFragmentsCount + 1> execObject;

    reinterpret_cast<uint32_t *>(this->gpuAddress)[0] = 0x05000000;
    reinterpret_cast<uint32_t *>(this->gpuAddress)[1] = 0x00000000;

    execObject.resize(numberOfBos + 1);

    uint32_t boIndex = 0;
    for (boIndex = 0; boIndex < (uint32_t)numberOfBos; boIndex++) {
        boToPin[boIndex]->fillExecObject(execObject[boIndex], drmContextId);
    }

    this->fillExecObject(execObject[boIndex], drmContextId);

    execbuf.buffers_ptr = reinterpret_cast<uintptr_t>(&execObject[0]);
    execbuf.buffer_count = boIndex + 1;
    execbuf.batch_len = alignUp(static_cast<uint32_t>(sizeof(uint32_t)), 8);
    execbuf.rsvd1 = drmContextId;

    int err = 0;
    int ret = this->drm->ioctl(DRM_IOCTL_I915_GEM_EXECBUFFER2, &execbuf);
    if (ret != 0) {
        err = this->drm->getErrno();
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(I915_GEM_EXECBUFFER2) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
    }

    return err;
}

} // namespace NEO

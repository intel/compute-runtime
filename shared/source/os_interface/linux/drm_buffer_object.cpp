/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_buffer_object.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/os_time_linux.h"
#include "shared/source/utilities/stackvec.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

namespace NEO {

BufferObject::BufferObject(Drm *drm, int handle, size_t size) : drm(drm), refCount(1), handle(handle), size(size), isReused(false) {
    this->tiling_mode = I915_TILING_NONE;
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

    return set_tiling.tiling_mode == mode;
}

void BufferObject::fillExecObject(drm_i915_gem_exec_object2 &execObject, uint32_t drmContextId) {
    execObject.handle = this->handle;
    execObject.relocation_count = 0; //No relocations, we are SoftPinning
    execObject.relocs_ptr = 0ul;
    execObject.alignment = 0;
    execObject.offset = this->gpuAddress;
    execObject.flags = EXEC_OBJECT_PINNED | EXEC_OBJECT_SUPPORTS_48B_ADDRESS;
    execObject.rsvd1 = drmContextId;
    execObject.rsvd2 = 0;
}

int BufferObject::pin(BufferObject *const boToPin[], size_t numberOfBos, uint32_t drmContextId) {
    reinterpret_cast<uint32_t *>(this->gpuAddress)[0] = 0x05000000;
    reinterpret_cast<uint32_t *>(this->gpuAddress)[1] = 0x00000000;
    StackVec<drm_i915_gem_exec_object2, maxFragmentsCount + 1> execObject(numberOfBos + 1);
    return this->exec(4u, 0u, 0u, false, drmContextId, boToPin, numberOfBos, &execObject[0]);
}

} // namespace NEO

/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "drm/i915_drm.h"

namespace NEO {

uint32_t IoctlHelper::ioctl(Drm *drm, unsigned long request, void *arg) {
    return drm->ioctl(request, arg);
}

static_assert(sizeof(ExecObject) == sizeof(drm_i915_gem_exec_object2));

void IoctlHelper::fillExecObject(ExecObject &execObject, uint32_t handle, uint64_t gpuAddress, uint32_t drmContextId, bool bindInfo, bool isMarkedForCapture) {

    auto drmExecObject = reinterpret_cast<drm_i915_gem_exec_object2 *>(execObject.data);
    drmExecObject->handle = handle;
    drmExecObject->relocation_count = 0; //No relocations, we are SoftPinning
    drmExecObject->relocs_ptr = 0ul;
    drmExecObject->alignment = 0;
    drmExecObject->offset = gpuAddress;
    drmExecObject->flags = EXEC_OBJECT_PINNED | EXEC_OBJECT_SUPPORTS_48B_ADDRESS;

    if (DebugManager.flags.UseAsyncDrmExec.get() == 1) {
        drmExecObject->flags |= EXEC_OBJECT_ASYNC;
    }

    if (isMarkedForCapture) {
        drmExecObject->flags |= EXEC_OBJECT_CAPTURE;
    }
    drmExecObject->rsvd1 = drmContextId;
    drmExecObject->rsvd2 = 0;

    if (bindInfo) {
        drmExecObject->handle = 0u;
    }
}

void IoctlHelper::logExecObject(const ExecObject &execObject, std::stringstream &logger, size_t size) {
    auto drmExecObject = reinterpret_cast<const drm_i915_gem_exec_object2 *>(execObject.data);
    logger << "Buffer Object = { handle: BO-" << drmExecObject->handle
           << ", address range: 0x" << reinterpret_cast<void *>(drmExecObject->offset)
           << " - 0x" << reinterpret_cast<void *>(ptrOffset(drmExecObject->offset, size))
           << ", flags: " << std::hex << drmExecObject->flags << std::dec
           << ", size: " << size << " }\n";
}
} // namespace NEO

/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/os_context.h"

#include "drm/i915_drm.h"

#include <sstream>

namespace NEO {

uint32_t IoctlHelper::ioctl(Drm *drm, DrmIoctl request, void *arg) {
    return drm->ioctl(request, arg);
}

void IoctlHelper::fillExecObject(ExecObject &execObject, uint32_t handle, uint64_t gpuAddress, uint32_t drmContextId, bool bindInfo, bool isMarkedForCapture) {

    auto &drmExecObject = *reinterpret_cast<drm_i915_gem_exec_object2 *>(execObject.data);
    drmExecObject.handle = handle;
    drmExecObject.relocation_count = 0; //No relocations, we are SoftPinning
    drmExecObject.relocs_ptr = 0ul;
    drmExecObject.alignment = 0;
    drmExecObject.offset = gpuAddress;
    drmExecObject.flags = EXEC_OBJECT_PINNED | EXEC_OBJECT_SUPPORTS_48B_ADDRESS;

    if (DebugManager.flags.UseAsyncDrmExec.get() == 1) {
        drmExecObject.flags |= static_cast<decltype(drmExecObject.flags)>(EXEC_OBJECT_ASYNC);
    }

    if (isMarkedForCapture) {
        drmExecObject.flags |= static_cast<decltype(drmExecObject.flags)>(EXEC_OBJECT_CAPTURE);
    }
    drmExecObject.rsvd1 = drmContextId;
    drmExecObject.rsvd2 = 0;

    if (bindInfo) {
        drmExecObject.handle = 0u;
    }
}

void IoctlHelper::logExecObject(const ExecObject &execObject, std::stringstream &logger, size_t size) {
    auto &drmExecObject = *reinterpret_cast<const drm_i915_gem_exec_object2 *>(execObject.data);
    logger << "Buffer Object = { handle: BO-" << drmExecObject.handle
           << ", address range: 0x" << reinterpret_cast<void *>(drmExecObject.offset)
           << " - 0x" << reinterpret_cast<void *>(ptrOffset(drmExecObject.offset, size))
           << ", flags: " << std::hex << drmExecObject.flags << std::dec
           << ", size: " << size << " }\n";
}

void IoctlHelper::fillExecBuffer(ExecBuffer &execBuffer, uintptr_t buffersPtr, uint32_t bufferCount, uint32_t startOffset, uint32_t size, uint64_t flags, uint32_t drmContextId) {
    auto &drmExecBuffer = *reinterpret_cast<drm_i915_gem_execbuffer2 *>(execBuffer.data);
    drmExecBuffer.buffers_ptr = buffersPtr;
    drmExecBuffer.buffer_count = bufferCount;
    drmExecBuffer.batch_start_offset = startOffset;
    drmExecBuffer.batch_len = size;
    drmExecBuffer.flags = flags;
    drmExecBuffer.rsvd1 = drmContextId;
}

void IoctlHelper::logExecBuffer(const ExecBuffer &execBuffer, std::stringstream &logger) {
    auto &drmExecBuffer = *reinterpret_cast<const drm_i915_gem_execbuffer2 *>(execBuffer.data);
    logger << "drm_i915_gem_execbuffer2 { "
           << "buffer_ptr: " + std::to_string(drmExecBuffer.buffers_ptr)
           << ", buffer_count: " + std::to_string(drmExecBuffer.buffer_count)
           << ", batch_start_offset: " + std::to_string(drmExecBuffer.batch_start_offset)
           << ", batch_len: " + std::to_string(drmExecBuffer.batch_len)
           << ", flags: " + std::to_string(drmExecBuffer.flags)
           << ", rsvd1: " + std::to_string(drmExecBuffer.rsvd1)
           << " }\n";
}

unsigned int IoctlHelper::getIoctlRequestValueBase(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::GemExecbuffer2:
        return DRM_IOCTL_I915_GEM_EXECBUFFER2;
    case DrmIoctl::GemWait:
        return DRM_IOCTL_I915_GEM_WAIT;
    case DrmIoctl::GemClose:
        return DRM_IOCTL_GEM_CLOSE;
    case DrmIoctl::GemUserptr:
        return DRM_IOCTL_I915_GEM_USERPTR;
    case DrmIoctl::GemCreate:
        return DRM_IOCTL_I915_GEM_CREATE;
    case DrmIoctl::GemSetDomain:
        return DRM_IOCTL_I915_GEM_SET_DOMAIN;
    case DrmIoctl::GemSetTiling:
        return DRM_IOCTL_I915_GEM_SET_TILING;
    case DrmIoctl::GemGetTiling:
        return DRM_IOCTL_I915_GEM_GET_TILING;
    case DrmIoctl::GemContextCreateExt:
        return DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT;
    case DrmIoctl::GemContextDestroy:
        return DRM_IOCTL_I915_GEM_CONTEXT_DESTROY;
    case DrmIoctl::RegRead:
        return DRM_IOCTL_I915_REG_READ;
    case DrmIoctl::GetResetStats:
        return DRM_IOCTL_I915_GET_RESET_STATS;
    case DrmIoctl::GemContextGetparam:
        return DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM;
    case DrmIoctl::GemContextSetparam:
        return DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM;
    case DrmIoctl::Query:
        return DRM_IOCTL_I915_QUERY;
    case DrmIoctl::GemMmap:
        return DRM_IOCTL_I915_GEM_MMAP;
    case DrmIoctl::PrimeFdToHandle:
        return DRM_IOCTL_PRIME_FD_TO_HANDLE;
    case DrmIoctl::PrimeHandleToFd:
        return DRM_IOCTL_PRIME_HANDLE_TO_FD;
    case DrmIoctl::GemMmapOffset:
        return DRM_IOCTL_I915_GEM_MMAP_OFFSET;
    case DrmIoctl::GemVmCreate:
        return DRM_IOCTL_I915_GEM_VM_CREATE;
    case DrmIoctl::GemVmDestroy:
        return DRM_IOCTL_I915_GEM_VM_DESTROY;
    case DrmIoctl::QueryEngineInfo:
        return DRM_I915_QUERY_ENGINE_INFO;
    case DrmIoctl::QueryMemoryRegions:
        return DRM_I915_QUERY_MEMORY_REGIONS;
    default:
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

uint32_t IoctlHelper::createDrmContext(Drm &drm, const OsContext &osContext, uint32_t drmVmId) {

    const auto numberOfCCS = drm.getRootDeviceEnvironment().getHardwareInfo()->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
    const bool debuggableContext = drm.isContextDebugSupported() && drm.getRootDeviceEnvironment().executionEnvironment.isDebuggingEnabled() && !osContext.isInternalEngine();
    const bool debuggableContextCooperative = debuggableContext && numberOfCCS > 0;
    auto drmContextId = drm.createDrmContext(drmVmId, drm.isVmBindAvailable(), osContext.isCooperativeEngine() || debuggableContextCooperative);
    if (drm.areNonPersistentContextsSupported()) {
        drm.setNonPersistentContext(drmContextId);
    }

    if (drm.getRootDeviceEnvironment().executionEnvironment.isDebuggingEnabled()) {
        drm.setUnrecoverableContext(drmContextId);
    }

    if (debuggableContext) {
        drm.setContextDebugFlag(drmContextId);
    }

    if (drm.isPreemptionSupported() && osContext.isLowPriority()) {
        drm.setLowPriorityContextParam(drmContextId);
    }
    return drmContextId;
}

} // namespace NEO

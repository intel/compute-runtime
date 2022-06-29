/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_buffer_object.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/os_time_linux.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/utilities/stackvec.h"

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

BufferObject::BufferObject(Drm *drm, uint64_t patIndex, int handle, size_t size, size_t maxOsContextCount) : drm(drm), refCount(1), handle(handle), size(size) {
    auto ioctlHelper = drm->getIoctlHelper();
    this->tilingMode = ioctlHelper->getDrmParamValue(DrmParam::TilingNone);
    this->lockedAddress = nullptr;
    this->patIndex = patIndex;

    perContextVmsUsed = drm->isPerContextVMRequired();
    requiresExplicitResidency = drm->hasPageFaultSupport();

    if (perContextVmsUsed) {
        bindInfo.resize(maxOsContextCount);
        for (auto &iter : bindInfo) {
            iter.fill(false);
        }
    } else {
        bindInfo.resize(1);
        bindInfo[0].fill(false);
    }
}

uint32_t BufferObject::getRefCount() const {
    return this->refCount.load();
}

void BufferObject::setAddress(uint64_t address) {
    auto gmmHelper = drm->getRootDeviceEnvironment().getGmmHelper();

    this->gpuAddress = gmmHelper->canonize(address);
}

bool BufferObject::close() {
    GemClose close{};
    close.handle = this->handle;

    PRINT_DEBUG_STRING(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "Calling gem close on handle: BO-%d\n", this->handle);

    auto ioctlHelper = this->drm->getIoctlHelper();
    int ret = ioctlHelper->ioctl(DrmIoctl::GemClose, &close);
    if (ret != 0) {
        int err = errno;
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(GEM_CLOSE) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
        DEBUG_BREAK_IF(true);
        return false;
    }

    this->handle = -1;

    return true;
}

int BufferObject::wait(int64_t timeoutNs) {
    if (this->drm->isVmBindAvailable()) {
        return 0;
    }

    int ret = this->drm->waitHandle(this->handle, -1);
    UNRECOVERABLE_IF(ret != 0);

    return ret;
}

bool BufferObject::setTiling(uint32_t mode, uint32_t stride) {
    if (this->tilingMode == mode) {
        return true;
    }

    GemSetTiling setTiling{};
    setTiling.handle = this->handle;
    setTiling.tilingMode = mode;
    setTiling.stride = stride;
    auto ioctlHelper = this->drm->getIoctlHelper();

    if (ioctlHelper->ioctl(DrmIoctl::GemSetTiling, &setTiling) != 0) {
        return false;
    }

    this->tilingMode = setTiling.tilingMode;

    return setTiling.tilingMode == mode;
}

uint32_t BufferObject::getOsContextId(OsContext *osContext) {
    return perContextVmsUsed ? osContext->getContextId() : 0u;
}

void BufferObject::fillExecObject(ExecObject &execObject, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId) {
    const auto osContextId = drm->isPerContextVMRequired() ? osContext->getContextId() : 0;

    auto ioctlHelper = drm->getIoctlHelper();
    ioctlHelper->fillExecObject(execObject, this->handle, this->gpuAddress, drmContextId, this->bindInfo[osContextId][vmHandleId], this->isMarkedForCapture());
}

int BufferObject::exec(uint32_t used, size_t startOffset, unsigned int flags, bool requiresCoherency, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId,
                       BufferObject *const residency[], size_t residencyCount, ExecObject *execObjectsStorage, uint64_t completionGpuAddress, uint32_t completionValue) {
    for (size_t i = 0; i < residencyCount; i++) {
        residency[i]->fillExecObject(execObjectsStorage[i], osContext, vmHandleId, drmContextId);
    }
    this->fillExecObject(execObjectsStorage[residencyCount], osContext, vmHandleId, drmContextId);
    auto ioctlHelper = drm->getIoctlHelper();

    ExecBuffer execbuf{};
    ioctlHelper->fillExecBuffer(execbuf, reinterpret_cast<uintptr_t>(execObjectsStorage),
                                static_cast<uint32_t>(residencyCount + 1u), static_cast<uint32_t>(startOffset),
                                alignUp(used, 8), flags, drmContextId);

    if (DebugManager.flags.PrintExecutionBuffer.get()) {
        PRINT_DEBUG_STRING(DebugManager.flags.PrintExecutionBuffer.get(), stdout, "Exec called with drmVmId = %u\n",
                           static_cast<const OsContextLinux *>(osContext)->getDrmVmIds().size() ? static_cast<const OsContextLinux *>(osContext)->getDrmVmIds()[vmHandleId] : 0);

        printExecutionBuffer(execbuf, residencyCount, execObjectsStorage, residency);
    }

    int ret = ioctlHelper->execBuffer(&execbuf, completionGpuAddress, completionValue);

    if (ret != 0) {
        int err = this->drm->getErrno();
        if (err == EOPNOTSUPP) {
            PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(I915_GEM_EXECBUFFER2) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
            return err;
        }

        evictUnusedAllocations(false, true);
        ret = ioctlHelper->execBuffer(&execbuf, completionGpuAddress, completionValue);
    }

    if (ret != 0) {
        const auto status = evictUnusedAllocations(true, true);
        if (status == MemoryOperationsStatus::GPU_HANG_DETECTED_DURING_OPERATION) {
            PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "Error! GPU hang detected in BufferObject::exec(). Returning %d\n", gpuHangDetected);
            return gpuHangDetected;
        }

        ret = ioctlHelper->execBuffer(&execbuf, completionGpuAddress, completionValue);
    }

    if (ret == 0) {
        return 0;
    }

    int err = this->drm->getErrno();
    PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "ioctl(I915_GEM_EXECBUFFER2) failed with %d. errno=%d(%s)\n", ret, err, strerror(err));
    return err;
}

MemoryOperationsStatus BufferObject::evictUnusedAllocations(bool waitForCompletion, bool isLockNeeded) {
    return static_cast<DrmMemoryOperationsHandler *>(this->drm->getRootDeviceEnvironment().memoryOperationsInterface.get())->evictUnusedAllocations(waitForCompletion, isLockNeeded);
}

void BufferObject::printBOBindingResult(OsContext *osContext, uint32_t vmHandleId, bool bind, int retVal) {
    if (retVal == 0) {
        if (bind) {
            PRINT_DEBUG_STRING(DebugManager.flags.PrintBOBindingResult.get(), stdout, "bind BO-%d to VM %u, drmVmId = %u, range: %llx - %llx, size: %lld, result: %d\n",
                               this->handle, vmHandleId, static_cast<const OsContextLinux *>(osContext)->getDrmVmIds().size() ? static_cast<const OsContextLinux *>(osContext)->getDrmVmIds()[vmHandleId] : 0, this->gpuAddress, ptrOffset(this->gpuAddress, this->size), this->size, retVal);
        } else {
            PRINT_DEBUG_STRING(DebugManager.flags.PrintBOBindingResult.get(), stdout, "unbind BO-%d from VM %u, drmVmId = %u, range: %llx - %llx, size: %lld, result: %d\n",
                               this->handle, vmHandleId, static_cast<const OsContextLinux *>(osContext)->getDrmVmIds().size() ? static_cast<const OsContextLinux *>(osContext)->getDrmVmIds()[vmHandleId] : 0, this->gpuAddress, ptrOffset(this->gpuAddress, this->size), this->size, retVal);
        }
    } else {
        auto err = this->drm->getErrno();
        if (bind) {
            PRINT_DEBUG_STRING(DebugManager.flags.PrintBOBindingResult.get(), stderr, "bind BO-%d to VM %u, drmVmId = %u, range: %llx - %llx, size: %lld, result: %d, errno: %d(%s)\n",
                               this->handle, vmHandleId, static_cast<const OsContextLinux *>(osContext)->getDrmVmIds().size() ? static_cast<const OsContextLinux *>(osContext)->getDrmVmIds()[vmHandleId] : 0, this->gpuAddress, ptrOffset(this->gpuAddress, this->size), this->size, retVal, err, strerror(err));
        } else {
            PRINT_DEBUG_STRING(DebugManager.flags.PrintBOBindingResult.get(), stderr, "unbind BO-%d from VM %u, drmVmId = %u, range: %llx - %llx, size: %lld, result: %d, errno: %d(%s)\n",
                               this->handle, vmHandleId, static_cast<const OsContextLinux *>(osContext)->getDrmVmIds().size() ? static_cast<const OsContextLinux *>(osContext)->getDrmVmIds()[vmHandleId] : 0, this->gpuAddress, ptrOffset(this->gpuAddress, this->size), this->size, retVal, err, strerror(err));
        }
    }
}

int BufferObject::bind(OsContext *osContext, uint32_t vmHandleId) {
    int retVal = 0;
    auto contextId = getOsContextId(osContext);
    if (!this->bindInfo[contextId][vmHandleId]) {
        retVal = this->drm->bindBufferObject(osContext, vmHandleId, this);
        if (DebugManager.flags.PrintBOBindingResult.get()) {
            printBOBindingResult(osContext, vmHandleId, true, retVal);
        }
        if (!retVal) {
            this->bindInfo[contextId][vmHandleId] = true;
        }
    }
    return retVal;
}

int BufferObject::unbind(OsContext *osContext, uint32_t vmHandleId) {
    int retVal = 0;
    auto contextId = getOsContextId(osContext);
    if (this->bindInfo[contextId][vmHandleId]) {
        retVal = this->drm->unbindBufferObject(osContext, vmHandleId, this);
        if (DebugManager.flags.PrintBOBindingResult.get()) {
            printBOBindingResult(osContext, vmHandleId, false, retVal);
        }
        if (!retVal) {
            this->bindInfo[contextId][vmHandleId] = false;
        }
    }
    return retVal;
}

void BufferObject::printExecutionBuffer(ExecBuffer &execbuf, const size_t &residencyCount, ExecObject *execObjectsStorage, BufferObject *const residency[]) {
    auto ioctlHelper = drm->getIoctlHelper();
    std::stringstream logger;
    ioctlHelper->logExecBuffer(execbuf, logger);

    size_t i;
    for (i = 0; i < residencyCount; i++) {
        ioctlHelper->logExecObject(execObjectsStorage[i], logger, residency[i]->peekSize());
    }
    logger << "Command ";
    ioctlHelper->logExecObject(execObjectsStorage[i], logger, this->peekSize());

    printf("%s\n", logger.str().c_str());
}

int bindBOsWithinContext(BufferObject *const boToPin[], size_t numberOfBos, OsContext *osContext, uint32_t vmHandleId) {
    auto retVal = 0;

    for (auto drmIterator = 0u; drmIterator < osContext->getDeviceBitfield().size(); drmIterator++) {
        if (osContext->getDeviceBitfield().test(drmIterator)) {
            for (size_t i = 0; i < numberOfBos; i++) {
                retVal |= boToPin[i]->bind(osContext, drmIterator);
            }
        }
    }

    return retVal;
}

int BufferObject::pin(BufferObject *const boToPin[], size_t numberOfBos, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId) {
    auto retVal = 0;

    if (this->drm->isVmBindAvailable()) {
        retVal = bindBOsWithinContext(boToPin, numberOfBos, osContext, vmHandleId);
    } else {
        StackVec<ExecObject, maxFragmentsCount + 1> execObject(numberOfBos + 1);
        retVal = this->exec(4u, 0u, 0u, false, osContext, vmHandleId, drmContextId, boToPin, numberOfBos, &execObject[0], 0, 0);
    }

    return retVal;
}

int BufferObject::validateHostPtr(BufferObject *const boToPin[], size_t numberOfBos, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId) {
    auto retVal = 0;

    if (this->drm->isVmBindAvailable()) {
        for (size_t i = 0; i < numberOfBos; i++) {
            retVal = boToPin[i]->bind(osContext, vmHandleId);
            if (retVal) {
                break;
            }
        }
    } else {
        StackVec<ExecObject, maxFragmentsCount + 1> execObject(numberOfBos + 1);
        retVal = this->exec(4u, 0u, 0u, false, osContext, vmHandleId, drmContextId, boToPin, numberOfBos, &execObject[0], 0, 0);
    }

    return retVal;
}

void BufferObject::addBindExtHandle(uint32_t handle) {
    bindExtHandles.push_back(handle);
}

} // namespace NEO

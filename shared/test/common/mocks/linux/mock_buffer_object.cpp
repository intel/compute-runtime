/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/linux/mock_buffer_object.h"

#include <cerrno>

namespace NEO {

MockBufferObjectHandleWrapper::MockBufferObjectHandleWrapper(BufferObjectHandleWrapper &&rhs)
    : BufferObjectHandleWrapper(std::move(rhs)) {}

MockBufferObject::MockBufferObject(uint32_t rootDeviceIndex, Drm *drm) : BufferObject(rootDeviceIndex, drm, CommonConstants::unsupportedPatIndex, 0, 0, 1) {
}

void MockBufferObject::setSize(size_t mockSize) {
    size = mockSize;
}

int MockBufferObject::exec(uint32_t used, size_t startOffset, unsigned int flags, bool requiresCoherency, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId,
                           BufferObject *const residency[], size_t residencyCount, ExecObject *execObjectsStorage, uint64_t completionGpuAddress, TaskCountType completionValue) {
    if (execReturnValue) {
        return *execReturnValue;
    }
    passedExecParams.push_back({completionGpuAddress, completionValue});
    return BufferObject::exec(used, startOffset, flags, requiresCoherency, osContext, vmHandleId, drmContextId,
                              residency, residencyCount, execObjectsStorage, completionGpuAddress, completionValue);
}

int MockBufferObject::unbind(OsContext *osContext, uint32_t vmHandleId) {
    errno = 0;
    if (unbindErrno != 0) {
        errno = unbindErrno;
        return -1;
    }
    return BufferObject::unbind(osContext, vmHandleId);
}

} // namespace NEO

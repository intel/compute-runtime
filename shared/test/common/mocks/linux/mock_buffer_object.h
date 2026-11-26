/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_buffer_object.h"

#include <optional>

namespace NEO {

class MockBufferObjectHandleWrapper : public BufferObjectHandleWrapper {
  public:
    using BufferObjectHandleWrapper::boHandle;
    using BufferObjectHandleWrapper::BufferObjectHandleWrapper;
    using BufferObjectHandleWrapper::controlBlock;

    MockBufferObjectHandleWrapper(BufferObjectHandleWrapper &&rhs);
};

class MockBufferObject : public BufferObject {
  public:
    using BufferObject::bindExtHandles;
    using BufferObject::bindInfo;
    using BufferObject::BufferObject;
    using BufferObject::gpuAddress;
    using BufferObject::handle;
    using BufferObject::isaDebugDataHandle;
    using BufferObject::resourceClass;
    using BufferObject::size;

    struct ExecParams {
        uint64_t completionGpuAddress = 0;
        TaskCountType completionValue = 0;
    };

    void setSize(size_t mockSize);

    std::optional<int> execReturnValue;
    std::vector<ExecParams> passedExecParams{};
    MockBufferObject(uint32_t rootDeviceIndex, Drm *drm);
    int exec(uint32_t used, size_t startOffset, unsigned int flags, bool requiresCoherency, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId,
             BufferObject *const residency[], size_t residencyCount, ExecObject *execObjectsStorage, uint64_t completionGpuAddress, TaskCountType completionValue) override;

    int unbind(OsContext *osContext, uint32_t vmHandleId) override;

    int unbindErrno = 0;
};

} // namespace NEO

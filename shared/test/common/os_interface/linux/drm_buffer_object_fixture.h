/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_default.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "drm/i915_drm.h"

#include <memory>

class TestedBufferObject : public BufferObject {
  public:
    using BufferObject::handle;
    TestedBufferObject(Drm *drm) : BufferObject(drm, 1, 0, 1) {
    }

    TestedBufferObject(Drm *drm, size_t size) : BufferObject(drm, 1, size, 1) {
    }

    void tileBy(uint32_t mode) {
        this->tilingMode = mode;
    }

    void fillExecObject(drm_i915_gem_exec_object2 &execObject, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId) override {
        BufferObject::fillExecObject(execObject, osContext, vmHandleId, drmContextId);
        execObjectPointerFilled = &execObject;
    }

    void setSize(size_t size) {
        this->size = size;
    }

    int exec(uint32_t used, size_t startOffset, unsigned int flags, bool requiresCoherency, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId,
             BufferObject *const residency[], size_t residencyCount, drm_i915_gem_exec_object2 *execObjectsStorage, uint64_t completionGpuAddress, uint32_t completionValue) override {
        this->receivedCompletionGpuAddress = completionGpuAddress;
        this->receivedCompletionValue = completionValue;
        this->execCalled++;
        return BufferObject::exec(used, startOffset, flags, requiresCoherency, osContext, vmHandleId, drmContextId, residency, residencyCount, execObjectsStorage, completionGpuAddress, completionValue);
    }

    uint64_t receivedCompletionGpuAddress = 0;
    drm_i915_gem_exec_object2 *execObjectPointerFilled = nullptr;
    uint32_t receivedCompletionValue = 0;
    uint32_t execCalled = 0;
};

template <typename DrmClass>
class DrmBufferObjectFixture {
  public:
    std::unique_ptr<DrmClass> mock;
    TestedBufferObject *bo;
    drm_i915_gem_exec_object2 execObjectsStorage[256];
    std::unique_ptr<OsContextLinux> osContext;

    void SetUp() {
        this->mock = std::make_unique<DrmClass>(*executionEnvironment.rootDeviceEnvironments[0]);
        ASSERT_NE(nullptr, this->mock);
        executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock.get(), 0u);
        osContext.reset(new OsContextLinux(*this->mock, 0u, EngineDescriptorHelper::getDefaultDescriptor()));
        this->mock->reset();
        bo = new TestedBufferObject(this->mock.get());
        ASSERT_NE(nullptr, bo);
    }

    void TearDown() {
        delete bo;
        if (this->mock->ioctl_expected.total >= 0) {
            EXPECT_EQ(this->mock->ioctl_expected.total, this->mock->ioctl_cnt.total);
        }
        mock->reset();
        osContext.reset(nullptr);
        mock.reset(nullptr);
    }
    MockExecutionEnvironment executionEnvironment;
};

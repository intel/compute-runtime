/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_default.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

class TestedBufferObject : public BufferObject {
  public:
    using BufferObject::handle;
    using BufferObject::tilingMode;

    TestedBufferObject(uint32_t rootDeviceIndex, Drm *drm) : TestedBufferObject(rootDeviceIndex, drm, 0) {
    }

    TestedBufferObject(uint32_t rootDeviceIndex, Drm *drm, size_t size) : BufferObject(rootDeviceIndex, drm, 3, 1, size, 1) {
    }

    void fillExecObject(ExecObject &execObject, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId) override {
        BufferObject::fillExecObject(execObject, osContext, vmHandleId, drmContextId);
        execObjectPointerFilled = &execObject;
    }

    void setSize(size_t size) {
        this->size = size;
    }

    int exec(uint32_t used, size_t startOffset, unsigned int flags, bool requiresCoherency, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId,
             BufferObject *const residency[], size_t residencyCount, ExecObject *execObjectsStorage, uint64_t completionGpuAddress, TaskCountType completionValue) override {
        this->receivedCompletionGpuAddress = completionGpuAddress;
        this->receivedCompletionValue = completionValue;
        this->execCalled++;
        return BufferObject::exec(used, startOffset, flags, requiresCoherency, osContext, vmHandleId, drmContextId, residency, residencyCount, execObjectsStorage, completionGpuAddress, completionValue);
    }

    MemoryOperationsStatus evictUnusedAllocations(bool waitForCompletion, bool isLockNeeded) override {
        if (callBaseEvictUnusedAllocations) {
            return BufferObject::evictUnusedAllocations(waitForCompletion, isLockNeeded);
        }

        if (!waitForCompletion) {
            return MemoryOperationsStatus::success;
        }

        return MemoryOperationsStatus::gpuHangDetectedDuringOperation;
    }

    uint64_t receivedCompletionGpuAddress = 0;
    ExecObject *execObjectPointerFilled = nullptr;
    TaskCountType receivedCompletionValue = 0;
    uint32_t execCalled = 0;
    bool callBaseEvictUnusedAllocations{true};
};

template <typename DrmClass>
class DrmBufferObjectFixture {
  public:
    std::unique_ptr<DrmClass> mock;
    TestedBufferObject *bo = nullptr;
    ExecObject execObjectsStorage[256]{};
    std::unique_ptr<OsContextLinux> osContext;
    const uint32_t rootDeviceIndex = 0u;

    void setUp() {
        this->mock = DrmClass::create(*executionEnvironment.rootDeviceEnvironments[0]);
        ASSERT_NE(nullptr, this->mock);
        executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock.get(), 0u, false);
        osContext.reset(new OsContextLinux(*this->mock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor()));
        this->mock->reset();
        bo = new TestedBufferObject(0, this->mock.get());
        ASSERT_NE(nullptr, bo);
    }

    void tearDown() {
        delete bo;
        if (this->mock->ioctlExpected.total >= 0) {
            EXPECT_EQ(this->mock->ioctlExpected.total, this->mock->ioctlCnt.total);
        }
        mock->reset();
        osContext.reset(nullptr);
        mock.reset(nullptr);
    }
    MockExecutionEnvironment executionEnvironment;
};

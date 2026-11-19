/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

#include <memory>

namespace NEO {
namespace LEO {
namespace ult {

struct BufferDestructorTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        capturingContext = std::make_unique<CapturingContext>(driverHandle.get(), clDevice->getL0Handle());
        leoContext = std::make_unique<Context>(nullptr, capturingContext->toHandle(), 1, &clDeviceId, true);
    }

    void TearDown() override {
        leoContext.reset();
        capturingContext.reset();
        Test<OclFixture>::TearDown();
    }

    Buffer *createBuffer(cl_mem_flags flags, void *usmPtr, bool usesExternalHandle, void *cpuPtr) {
        auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &clDevice->getDevice());
        return new Buffer(leoContext.get(), memoryProperties, flags, usmPtr, cpuPtr, 64u, usesExternalHandle);
    }

    Buffer *createBuffer(void *usmPtr, bool usesExternalHandle = false, void *cpuPtr = nullptr) {
        return createBuffer(CL_MEM_READ_WRITE, usmPtr, usesExternalHandle, cpuPtr);
    }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<CapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;

    uint64_t dummyStorage = 0u;
};

struct ExternalMemoryBufferTest : public BufferDestructorTest {
    void SetUp() override {
        BufferDestructorTest::SetUp();
        supportedExternalMemoryHandleKey = clDevice->getDeviceInfo().externalMemorySharing;
    }

    cl_mem_properties supportedExternalMemoryHandleKey = 0;
};

} // namespace ult
} // namespace LEO
} // namespace NEO

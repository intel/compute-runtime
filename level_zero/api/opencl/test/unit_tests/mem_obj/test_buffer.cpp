/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

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

    Buffer *createBuffer(void *usmPtr, bool usesExternalHandle = false, void *cpuPtr = nullptr) {
        auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &clDevice->getDevice());
        return new Buffer(leoContext.get(), memoryProperties, CL_MEM_READ_WRITE, usmPtr, cpuPtr, 64u, usesExternalHandle);
    }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<CapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;

    uint64_t dummyStorage = 0u;
};

TEST_F(BufferDestructorTest, givenBufferWithoutSvmAndWithoutExternalHandleWhenDestroyedThenZeMemFreeExtIsCalledWithBlockingPolicy) {
    void *usmPtr = &dummyStorage;
    auto buffer = createBuffer(usmPtr);
    buffer->setUsesSvm(false);

    delete buffer;

    ASSERT_EQ(1u, capturingContext->freeMemExtArgs.count());
    const auto &freeMem = capturingContext->freeMemExtArgs[0];
    EXPECT_EQ(ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_BLOCKING_FREE, freeMem.freePolicy);
    EXPECT_EQ(usmPtr, freeMem.ptr);
}

TEST_F(BufferDestructorTest, givenBufferWithCpuAllocationAndSvmWhenDestroyedThenMemObjCallsZeMemFreeExtWithBlockingPolicy) {
    uint64_t dummyCpuStorage = 0u;
    auto buffer = createBuffer(&dummyStorage, false, &dummyCpuStorage);
    buffer->setUsesSvm(true);

    delete buffer;

    ASSERT_EQ(1u, capturingContext->freeMemExtArgs.count());
    const auto &freeMem = capturingContext->freeMemExtArgs[0];
    EXPECT_EQ(ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_BLOCKING_FREE, freeMem.freePolicy);
    EXPECT_EQ(&dummyCpuStorage, freeMem.ptr);
}

TEST_F(BufferDestructorTest, givenBufferWithSvmWhenDestroyedThenZeMemFreeExtIsNotCalled) {
    void *usmPtr = &dummyStorage;
    auto buffer = createBuffer(usmPtr);
    buffer->setUsesSvm(true);

    delete buffer;

    EXPECT_FALSE(capturingContext->freeMemExtArgs.wasCalled());
}

TEST_F(BufferDestructorTest, givenBufferWithExternalHandleWhenDestroyedThenZeMemFreeExtIsNotCalled) {
    void *usmPtr = &dummyStorage;
    constexpr bool externalHandle = true;
    auto buffer = createBuffer(usmPtr, externalHandle);

    delete buffer;

    EXPECT_FALSE(capturingContext->freeMemExtArgs.wasCalled());
}

} // namespace ult
} // namespace LEO
} // namespace NEO

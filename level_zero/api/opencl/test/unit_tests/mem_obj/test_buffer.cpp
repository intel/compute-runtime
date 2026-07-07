/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

struct CapturingFreeMemL0Context : public L0::ult::Mock<L0::Context> {
    CapturingFreeMemL0Context(L0::DriverHandle *driverHandle, ze_device_handle_t deviceHandle) : L0::ult::Mock<L0::Context>(driverHandle) {
        this->devices[0] = deviceHandle;
        this->numDevices = 1;
    }

    ze_result_t freeMem(const void *ptr) override {
        ++freeMemCallCount;
        lastFreedPtr = ptr;
        return ZE_RESULT_SUCCESS;
    }

    uint32_t freeMemCallCount = 0u;
    const void *lastFreedPtr = nullptr;
};

struct BufferDestructorTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        l0Context = std::make_unique<CapturingFreeMemL0Context>(driverHandle.get(), clDevice->getL0Handle());
        leoContext = std::make_unique<Context>(nullptr, l0Context->toHandle(), 1, &clDeviceId, true);
    }

    void TearDown() override {
        leoContext.reset();
        l0Context.reset();
        Test<OclFixture>::TearDown();
    }

    Buffer *createBuffer(void *usmPtr, bool usesExternalHandle = false) {
        auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &clDevice->getDevice());
        return new Buffer(leoContext.get(), memoryProperties, CL_MEM_READ_WRITE, usmPtr, nullptr, 64u, usesExternalHandle);
    }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<CapturingFreeMemL0Context> l0Context;
    std::unique_ptr<Context> leoContext;

    uint64_t dummyStorage = 0u;
};

TEST_F(BufferDestructorTest, givenBufferWithoutSvmAndWithoutExternalHandleWhenDestroyedThenZeMemFreeIsCalled) {
    void *usmPtr = &dummyStorage;
    auto buffer = createBuffer(usmPtr);
    buffer->setUsesSvm(false);

    delete buffer;

    EXPECT_EQ(1u, l0Context->freeMemCallCount);
    EXPECT_EQ(usmPtr, l0Context->lastFreedPtr);
}

TEST_F(BufferDestructorTest, givenBufferWithSvmWhenDestroyedThenZeMemFreeIsNotCalled) {
    void *usmPtr = &dummyStorage;
    auto buffer = createBuffer(usmPtr);
    buffer->setUsesSvm(true);

    delete buffer;

    EXPECT_EQ(0u, l0Context->freeMemCallCount);
}

TEST_F(BufferDestructorTest, givenBufferWithExternalHandleWhenDestroyedThenZeMemFreeIsNotCalled) {
    void *usmPtr = &dummyStorage;
    constexpr bool externalHandle = true;
    auto buffer = createBuffer(usmPtr, externalHandle);

    delete buffer;

    EXPECT_EQ(0u, l0Context->freeMemCallCount);
}

} // namespace ult
} // namespace LEO
} // namespace NEO

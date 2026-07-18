/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <memory>

namespace NEO {
namespace LEO {
namespace ult {

struct CapturingSharedMemL0Context : public L0::ult::Mock<L0::Context> {
    CapturingSharedMemL0Context(L0::DriverHandle *driverHandle, ze_device_handle_t deviceHandle) : L0::ult::Mock<L0::Context>(driverHandle) {
        this->devices[0] = deviceHandle;
        this->numDevices = 1;
    }

    ze_result_t allocSharedMem(ze_device_handle_t hDevice, const ze_device_mem_alloc_desc_t *deviceDesc,
                               const ze_host_mem_alloc_desc_t *hostDesc, size_t size, size_t alignment, void **ptr) override {
        ++allocSharedMemCallCount;
        capturedSize = size;
        capturedAlignment = alignment;
        *ptr = &dummyStorage;
        return ZE_RESULT_SUCCESS;
    }

    uint32_t allocSharedMemCallCount = 0u;
    size_t capturedSize = 0u;
    size_t capturedAlignment = 0u;
    uint64_t dummyStorage = 0u;
};

struct ClSvmAllocTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        l0Context = std::make_unique<CapturingSharedMemL0Context>(driverHandle.get(), clDevice->getL0Handle());
        leoContext = std::make_unique<Context>(nullptr, l0Context->toHandle(), 1, &clDeviceId, true);
    }

    void TearDown() override {
        leoContext.reset();
        l0Context.reset();
        Test<OclFixture>::TearDown();
    }

    cl_context getContext() { return leoContext.get(); }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<CapturingSharedMemL0Context> l0Context;
    std::unique_ptr<Context> leoContext;

    static constexpr size_t validSize = 4096u * sizeof(cl_long);
};

TEST_F(ClSvmAllocTest, givenAtomicsWithoutFineGrainBufferFlagWhenClSvmAllocThenReturnsNullAndDoesNotAllocate) {
    auto ptr = clSVMAlloc(getContext(), CL_MEM_SVM_ATOMICS, validSize, 0);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, l0Context->allocSharedMemCallCount);
}

TEST_F(ClSvmAllocTest, givenUseHostPtrFlagWhenClSvmAllocThenReturnsNullAndDoesNotAllocate) {
    auto ptr = clSVMAlloc(getContext(), CL_MEM_USE_HOST_PTR, validSize, 0);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, l0Context->allocSharedMemCallCount);
}

TEST_F(ClSvmAllocTest, givenCopyHostPtrFlagWhenClSvmAllocThenReturnsNullAndDoesNotAllocate) {
    auto ptr = clSVMAlloc(getContext(), CL_MEM_COPY_HOST_PTR, validSize, 0);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, l0Context->allocSharedMemCallCount);
}

TEST_F(ClSvmAllocTest, givenInvalidFlagsWhenClSvmAllocThenReturnsNullAndDoesNotAllocate) {
    auto ptr = clSVMAlloc(getContext(), 12345, validSize, 0);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, l0Context->allocSharedMemCallCount);
}

TEST_F(ClSvmAllocTest, givenZeroSizeWhenClSvmAllocThenReturnsNullAndDoesNotAllocate) {
    auto ptr = clSVMAlloc(getContext(), 0, 0, 0);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, l0Context->allocSharedMemCallCount);
}

TEST_F(ClSvmAllocTest, givenNonPowerOfTwoAlignmentWhenClSvmAllocThenReturnsNullAndDoesNotAllocate) {
    for (cl_uint alignment : {3u, 5u, 6u, 7u}) {
        auto ptr = clSVMAlloc(getContext(), 0, validSize, alignment);
        EXPECT_EQ(nullptr, ptr) << "alignment: " << alignment;
    }
    EXPECT_EQ(0u, l0Context->allocSharedMemCallCount);
}

TEST_F(ClSvmAllocTest, givenSizeExceedingMaxMemAllocSizeWhenClSvmAllocThenReturnsNullAndDoesNotAllocate) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.AllowUnrestrictedSize.set(0);

    auto maxMemAllocSize = clDevice->getSharedDeviceInfo().maxMemAllocSize;
    auto ptr = clSVMAlloc(getContext(), CL_MEM_READ_WRITE, static_cast<size_t>(maxMemAllocSize) + 1u, 0);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, l0Context->allocSharedMemCallCount);
}

TEST_F(ClSvmAllocTest, givenFineGrainBufferFlagWhenFineGrainedSvmIsUnsupportedThenClSvmAllocReturnsNullAndDoesNotAllocate) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.ForceFineGrainedSVMSupport.set(0);

    auto ptr = clSVMAlloc(getContext(), CL_MEM_SVM_FINE_GRAIN_BUFFER, validSize, 0);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(0u, l0Context->allocSharedMemCallCount);
}

TEST_F(ClSvmAllocTest, givenValidArgumentsWhenClSvmAllocThenForwardsToSharedAllocWithMatchingSizeAndAlignment) {
    auto ptr = clSVMAlloc(getContext(), CL_MEM_READ_WRITE, validSize, 8);
    EXPECT_EQ(&l0Context->dummyStorage, ptr);
    EXPECT_EQ(1u, l0Context->allocSharedMemCallCount);
    EXPECT_EQ(validSize, l0Context->capturedSize);
    EXPECT_EQ(8u, l0Context->capturedAlignment);
}

struct ClEnqueueSvmMapTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();

        auto &clDevices = platform->getDevices();
        ASSERT_FALSE(clDevices.empty());
        clDevice = clDevices[0].get();

        cl_int errcode = CL_SUCCESS;
        cl_device_id clDeviceId = clDevice;
        clContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
        ASSERT_EQ(CL_SUCCESS, errcode);
        ASSERT_NE(nullptr, clContext);

        context = castToObject<Context>(clContext);
        ASSERT_NE(nullptr, context);

        // Inject a mock immediate command list so queue synchronization is observable.
        commandQueue = new CommandQueue(context, clDevice, nullptr, mockCmdList.toHandle());
    }

    void TearDown() override {
        commandQueue->decRefApi();
        clReleaseContext(clContext);
        Test<OclFixture>::TearDown();
    }

    ClDevice *clDevice = nullptr;
    cl_context clContext = nullptr;
    Context *context = nullptr;
    CommandQueue *commandQueue = nullptr;
    L0::ult::Mock<L0::ult::CommandList> mockCmdList{};
    uint64_t svmStorage = 0u;
};

TEST_F(ClEnqueueSvmMapTest, givenBlockingMapWhenClEnqueueSVMMapThenQueueIsSynchronized) {
    auto retVal = clEnqueueSVMMap(commandQueue, CL_TRUE, CL_MAP_READ, &svmStorage, sizeof(svmStorage), 0, nullptr, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockCmdList.appendBarrierCalled);
    EXPECT_EQ(1u, mockCmdList.hostSynchronizeCalled);
}

TEST_F(ClEnqueueSvmMapTest, givenNonBlockingMapWhenClEnqueueSVMMapThenQueueIsNotSynchronized) {
    auto retVal = clEnqueueSVMMap(commandQueue, CL_FALSE, CL_MAP_READ, &svmStorage, sizeof(svmStorage), 0, nullptr, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockCmdList.appendBarrierCalled);
    EXPECT_EQ(0u, mockCmdList.hostSynchronizeCalled);
}

TEST_F(ClEnqueueSvmMapTest, givenBlockingMapWhenHostSynchronizeFailsThenErrorIsReturned) {
    mockCmdList.hostSynchronizeResult = ZE_RESULT_ERROR_DEVICE_LOST;

    auto retVal = clEnqueueSVMMap(commandQueue, CL_TRUE, CL_MAP_READ, &svmStorage, sizeof(svmStorage), 0, nullptr, nullptr);

    EXPECT_EQ(CL_DEVICE_NOT_AVAILABLE, retVal);
    EXPECT_EQ(1u, mockCmdList.hostSynchronizeCalled);
}

TEST_F(ClEnqueueSvmMapTest, givenClEnqueueSVMUnmapThenQueueIsNotSynchronized) {
    auto retVal = clEnqueueSVMUnmap(commandQueue, &svmStorage, 0, nullptr, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockCmdList.appendBarrierCalled);
    EXPECT_EQ(0u, mockCmdList.hostSynchronizeCalled);
}

} // namespace ult
} // namespace LEO
} // namespace NEO

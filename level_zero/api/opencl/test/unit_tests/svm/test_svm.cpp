/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <memory>

namespace NEO {
namespace LEO {
namespace ult {

struct ClSvmAllocTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();

        cl_int errcode = CL_SUCCESS;
        cl_device_id clDeviceId = clDevice;
        clContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
        ASSERT_EQ(CL_SUCCESS, errcode);
        ASSERT_NE(nullptr, clContext);

        svmManager = castToObject<Context>(clContext)->getL0Object()->getDriverHandle()->getSvmAllocsManager();
    }

    void TearDown() override {
        clReleaseContext(clContext);
        Test<OclFixture>::TearDown();
    }

    cl_context getContext() { return clContext; }

    ClDevice *clDevice = nullptr;
    cl_context clContext = nullptr;
    NEO::SVMAllocsManager *svmManager = nullptr;

    static constexpr size_t validSize = 4096u * sizeof(cl_long);
};

TEST_F(ClSvmAllocTest, givenAtomicsWithoutFineGrainBufferFlagWhenClSvmAllocThenReturnsNull) {
    auto ptr = clSVMAlloc(getContext(), CL_MEM_SVM_ATOMICS, validSize, 0);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(ClSvmAllocTest, givenUseHostPtrFlagWhenClSvmAllocThenReturnsNull) {
    auto ptr = clSVMAlloc(getContext(), CL_MEM_USE_HOST_PTR, validSize, 0);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(ClSvmAllocTest, givenCopyHostPtrFlagWhenClSvmAllocThenReturnsNull) {
    auto ptr = clSVMAlloc(getContext(), CL_MEM_COPY_HOST_PTR, validSize, 0);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(ClSvmAllocTest, givenInvalidFlagsWhenClSvmAllocThenReturnsNull) {
    auto ptr = clSVMAlloc(getContext(), 12345, validSize, 0);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(ClSvmAllocTest, givenZeroSizeWhenClSvmAllocThenReturnsNull) {
    auto ptr = clSVMAlloc(getContext(), 0, 0, 0);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(ClSvmAllocTest, givenNonPowerOfTwoAlignmentWhenClSvmAllocThenReturnsNull) {
    for (cl_uint alignment : {3u, 5u, 6u, 7u}) {
        auto ptr = clSVMAlloc(getContext(), 0, validSize, alignment);
        EXPECT_EQ(nullptr, ptr) << "alignment: " << alignment;
    }
}

TEST_F(ClSvmAllocTest, givenSizeExceedingMaxMemAllocSizeWhenClSvmAllocThenReturnsNull) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.AllowUnrestrictedSize.set(0);

    auto maxMemAllocSize = clDevice->getSharedDeviceInfo().maxMemAllocSize;
    auto ptr = clSVMAlloc(getContext(), CL_MEM_READ_WRITE, static_cast<size_t>(maxMemAllocSize) + 1u, 0);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(ClSvmAllocTest, givenFineGrainBufferFlagWhenFineGrainedSvmIsUnsupportedThenClSvmAllocReturnsNull) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.ForceFineGrainedSVMSupport.set(0);

    auto ptr = clSVMAlloc(getContext(), CL_MEM_SVM_FINE_GRAIN_BUFFER, validSize, 0);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(ClSvmAllocTest, givenValidArgumentsWhenClSvmAllocThenAllocationIsRegisteredAndFreedThroughSvmManager) {
    auto ptr = clSVMAlloc(getContext(), CL_MEM_READ_WRITE, validSize, 8);
    ASSERT_NE(nullptr, ptr);

    auto svmData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);
    EXPECT_EQ(validSize, svmData->size);

    clSVMFree(getContext(), ptr);
    EXPECT_EQ(nullptr, svmManager->getSVMAlloc(ptr));
}

TEST_F(ClSvmAllocTest, givenReadOnlyFlagWhenClSvmAllocThenAllocationIsRegistered) {
    auto ptr = clSVMAlloc(getContext(), CL_MEM_READ_ONLY, validSize, 0);
    ASSERT_NE(nullptr, ptr);
    EXPECT_NE(nullptr, svmManager->getSVMAlloc(ptr));
    clSVMFree(getContext(), ptr);
}

TEST_F(ClSvmAllocTest, givenSupportedAlignmentsWhenClSvmAllocThenAllocationSucceeds) {
    for (cl_uint alignment : {0u, 1u, 2u, 4u, 8u, 16u, 128u}) {
        auto ptr = clSVMAlloc(getContext(), CL_MEM_READ_WRITE, validSize, alignment);
        EXPECT_NE(nullptr, ptr) << "alignment: " << alignment;
        clSVMFree(getContext(), ptr);
    }
}

struct RecordingCommandList : public L0::ult::Mock<L0::ult::CommandList> {
    ze_result_t appendPageFaultCopy(NEO::GraphicsAllocation *dstAllocation, NEO::GraphicsAllocation *srcAllocation,
                                    size_t size, bool flushHost, size_t offset) override {
        ++appendPageFaultCopyCalled;
        pageFaultDst = dstAllocation;
        pageFaultSrc = srcAllocation;
        pageFaultSize = size;
        pageFaultFlushHost = flushHost;
        pageFaultOffset = offset;
        waitOnEventsCalledBeforeCopy = appendWaitOnEventsCalled;
        return appendPageFaultCopyResult;
    }

    NEO::GraphicsAllocation *pageFaultDst = nullptr;
    NEO::GraphicsAllocation *pageFaultSrc = nullptr;
    size_t pageFaultSize = 0u;
    size_t pageFaultOffset = 0u;
    bool pageFaultFlushHost = false;
    uint32_t waitOnEventsCalledBeforeCopy = 0u;
};

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

        svmManager = context->getL0Object()->getDriverHandle()->getSvmAllocsManager();

        // Inject a mock immediate command list so queue synchronization is observable.
        commandQueue = new CommandQueue(context, clDevice, nullptr, mockCmdList.toHandle());
    }

    void TearDown() override {
        if (deviceStorageData) {
            svmManager->removeSVMAlloc(*deviceStorageData);
        }
        commandQueue->decRefApi();
        clReleaseContext(clContext);
        Test<OclFixture>::TearDown();
    }

    void registerDeviceStorageAlloc(NEO::AllocationType allocationType = NEO::AllocationType::svmGpu) {
        cpuAllocation = std::make_unique<MockGraphicsAllocation>(&svmStorage, sizeof(svmStorage));
        gpuAllocation = std::make_unique<MockGraphicsAllocation>(&svmStorage, sizeof(svmStorage));
        gpuAllocation->allocationType = allocationType;

        deviceStorageData = std::make_unique<SvmAllocationData>(0u);
        deviceStorageData->gpuAllocations.addAllocation(gpuAllocation.get());
        deviceStorageData->cpuAllocation = cpuAllocation.get();
        deviceStorageData->size = sizeof(svmStorage);

        svmManager->insertSVMAlloc(*deviceStorageData);
    }

    ClDevice *clDevice = nullptr;
    cl_context clContext = nullptr;
    Context *context = nullptr;
    NEO::SVMAllocsManager *svmManager = nullptr;
    CommandQueue *commandQueue = nullptr;
    RecordingCommandList mockCmdList{};

    std::unique_ptr<MockGraphicsAllocation> cpuAllocation;
    std::unique_ptr<MockGraphicsAllocation> gpuAllocation;
    std::unique_ptr<SvmAllocationData> deviceStorageData;
    uint64_t svmStorage = 0u;
};

TEST_F(ClEnqueueSvmMapTest, givenBlockingMapWhenClEnqueueSVMMapThenQueueIsSynchronized) {
    auto retVal = clEnqueueSVMMap(commandQueue, CL_TRUE, CL_MAP_READ, &svmStorage, sizeof(svmStorage), 0, nullptr, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockCmdList.appendBarrierCalled);
    EXPECT_EQ(1u, mockCmdList.hostSynchronizeCalled);
    EXPECT_EQ(0u, mockCmdList.appendPageFaultCopyCalled);
}

TEST_F(ClEnqueueSvmMapTest, givenNonBlockingMapWhenClEnqueueSVMMapThenQueueIsNotSynchronized) {
    auto retVal = clEnqueueSVMMap(commandQueue, CL_FALSE, CL_MAP_READ, &svmStorage, sizeof(svmStorage), 0, nullptr, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockCmdList.appendBarrierCalled);
    EXPECT_EQ(0u, mockCmdList.hostSynchronizeCalled);
    EXPECT_EQ(0u, mockCmdList.appendPageFaultCopyCalled);
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
    EXPECT_EQ(0u, mockCmdList.appendPageFaultCopyCalled);
}

TEST_F(ClEnqueueSvmMapTest, givenDeviceStorageAllocationWhenClEnqueueSVMMapThenMigratesDeviceToHostAndRecordsOperation) {
    registerDeviceStorageAlloc();

    auto retVal = clEnqueueSVMMap(commandQueue, CL_FALSE, CL_MAP_WRITE, &svmStorage, sizeof(svmStorage), 0, nullptr, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockCmdList.appendPageFaultCopyCalled);
    EXPECT_EQ(cpuAllocation.get(), mockCmdList.pageFaultDst);
    EXPECT_EQ(gpuAllocation.get(), mockCmdList.pageFaultSrc);
    EXPECT_TRUE(mockCmdList.pageFaultFlushHost);
    EXPECT_EQ(sizeof(svmStorage), mockCmdList.pageFaultSize);

    auto mapOperation = svmManager->getSvmMapOperation(&svmStorage);
    ASSERT_NE(nullptr, mapOperation);
    EXPECT_FALSE(mapOperation->readOnlyMap);
}

TEST_F(ClEnqueueSvmMapTest, givenAlreadyMappedDeviceStorageWhenClEnqueueSVMMapAgainThenMigrationIsSkipped) {
    registerDeviceStorageAlloc();

    EXPECT_EQ(CL_SUCCESS, clEnqueueSVMMap(commandQueue, CL_FALSE, CL_MAP_WRITE, &svmStorage, sizeof(svmStorage), 0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, clEnqueueSVMMap(commandQueue, CL_FALSE, CL_MAP_WRITE, &svmStorage, sizeof(svmStorage), 0, nullptr, nullptr));

    EXPECT_EQ(1u, mockCmdList.appendPageFaultCopyCalled);
}

TEST_F(ClEnqueueSvmMapTest, givenReadOnlyMapWhenClEnqueueSVMUnmapThenMigrationIsSkippedAndOperationIsRemoved) {
    registerDeviceStorageAlloc();

    EXPECT_EQ(CL_SUCCESS, clEnqueueSVMMap(commandQueue, CL_FALSE, CL_MAP_READ, &svmStorage, sizeof(svmStorage), 0, nullptr, nullptr));
    EXPECT_EQ(1u, mockCmdList.appendPageFaultCopyCalled);

    EXPECT_EQ(CL_SUCCESS, clEnqueueSVMUnmap(commandQueue, &svmStorage, 0, nullptr, nullptr));

    EXPECT_EQ(1u, mockCmdList.appendPageFaultCopyCalled);
    EXPECT_EQ(nullptr, svmManager->getSvmMapOperation(&svmStorage));
}

TEST_F(ClEnqueueSvmMapTest, givenWriteMapWhenClEnqueueSVMUnmapThenMigratesHostToDeviceAndRemovesOperation) {
    registerDeviceStorageAlloc();

    EXPECT_EQ(CL_SUCCESS, clEnqueueSVMMap(commandQueue, CL_FALSE, CL_MAP_WRITE, &svmStorage, sizeof(svmStorage), 0, nullptr, nullptr));
    EXPECT_EQ(1u, mockCmdList.appendPageFaultCopyCalled);

    EXPECT_EQ(CL_SUCCESS, clEnqueueSVMUnmap(commandQueue, &svmStorage, 0, nullptr, nullptr));

    EXPECT_EQ(2u, mockCmdList.appendPageFaultCopyCalled);
    EXPECT_EQ(gpuAllocation.get(), mockCmdList.pageFaultDst);
    EXPECT_EQ(cpuAllocation.get(), mockCmdList.pageFaultSrc);
    EXPECT_FALSE(mockCmdList.pageFaultFlushHost);
    EXPECT_EQ(nullptr, svmManager->getSvmMapOperation(&svmStorage));
}

TEST_F(ClEnqueueSvmMapTest, givenBlockingDeviceStorageMapThenQueueIsSynchronizedAfterMigration) {
    registerDeviceStorageAlloc();

    auto retVal = clEnqueueSVMMap(commandQueue, CL_TRUE, CL_MAP_WRITE, &svmStorage, sizeof(svmStorage), 0, nullptr, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockCmdList.appendPageFaultCopyCalled);
    EXPECT_EQ(1u, mockCmdList.hostSynchronizeCalled);
}

TEST_F(ClEnqueueSvmMapTest, givenWaitListWhenDeviceStorageMapThenWaitsBeforeMigration) {
    registerDeviceStorageAlloc();

    cl_int errcode = CL_SUCCESS;
    cl_event userEvent = clCreateUserEvent(clContext, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, userEvent);

    auto retVal = clEnqueueSVMMap(commandQueue, CL_FALSE, CL_MAP_WRITE, &svmStorage, sizeof(svmStorage), 1, &userEvent, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockCmdList.appendPageFaultCopyCalled);
    EXPECT_EQ(1u, mockCmdList.appendWaitOnEventsCalled);
    EXPECT_EQ(1u, mockCmdList.waitOnEventsCalledBeforeCopy);

    clSetUserEventStatus(userEvent, CL_COMPLETE);
    clReleaseEvent(userEvent);
}

TEST_F(ClEnqueueSvmMapTest, givenZeroCopyAllocationWhenClEnqueueSVMMapThenMigrationIsSkipped) {
    registerDeviceStorageAlloc(NEO::AllocationType::svmZeroCopy);

    auto retVal = clEnqueueSVMMap(commandQueue, CL_FALSE, CL_MAP_WRITE, &svmStorage, sizeof(svmStorage), 0, nullptr, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, mockCmdList.appendPageFaultCopyCalled);
    EXPECT_EQ(1u, mockCmdList.appendBarrierCalled);
    EXPECT_EQ(nullptr, svmManager->getSvmMapOperation(&svmStorage));
}

TEST_F(ClEnqueueSvmMapTest, givenSubRegionMapAndUnmapWhenDeviceStorageThenOnlyThatRegionIsMigrated) {
    registerDeviceStorageAlloc();

    constexpr size_t regionOffset = sizeof(uint32_t);
    constexpr size_t regionSize = sizeof(uint32_t);
    auto regionPtr = ptrOffset(&svmStorage, regionOffset);

    EXPECT_EQ(CL_SUCCESS, clEnqueueSVMMap(commandQueue, CL_FALSE, CL_MAP_WRITE, regionPtr, regionSize, 0, nullptr, nullptr));
    EXPECT_EQ(1u, mockCmdList.appendPageFaultCopyCalled);
    EXPECT_EQ(cpuAllocation.get(), mockCmdList.pageFaultDst);
    EXPECT_EQ(gpuAllocation.get(), mockCmdList.pageFaultSrc);
    EXPECT_EQ(regionSize, mockCmdList.pageFaultSize);
    EXPECT_EQ(regionOffset, mockCmdList.pageFaultOffset);

    EXPECT_EQ(CL_SUCCESS, clEnqueueSVMUnmap(commandQueue, regionPtr, 0, nullptr, nullptr));
    EXPECT_EQ(2u, mockCmdList.appendPageFaultCopyCalled);
    EXPECT_EQ(gpuAllocation.get(), mockCmdList.pageFaultDst);
    EXPECT_EQ(cpuAllocation.get(), mockCmdList.pageFaultSrc);
    EXPECT_EQ(regionSize, mockCmdList.pageFaultSize);
    EXPECT_EQ(regionOffset, mockCmdList.pageFaultOffset);
    EXPECT_EQ(nullptr, svmManager->getSvmMapOperation(regionPtr));
}

} // namespace ult
} // namespace LEO
} // namespace NEO

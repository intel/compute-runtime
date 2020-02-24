/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/sync_buffer_handler.h"

#include "opencl/source/api/api.h"
#include "opencl/test/unit_test/fixtures/enqueue_handler_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "test.h"

using namespace NEO;

class MockSyncBufferHandler : public SyncBufferHandler {
  public:
    using SyncBufferHandler::bufferSize;
    using SyncBufferHandler::graphicsAllocation;
    using SyncBufferHandler::usedBufferSize;
};

class SyncBufferHandlerTest : public EnqueueHandlerTest {
  public:
    void SetUp() override {}
    void TearDown() override {}

    template <typename FamilyType>
    void SetUpT() {
        EnqueueHandlerTest::SetUp();
        kernelInternals = std::make_unique<MockKernelWithInternals>(*pClDevice, context);
        kernel = kernelInternals->mockKernel;
        kernel->executionType = KernelExecutionType::Concurrent;
        commandQueue = reinterpret_cast<MockCommandQueue *>(new MockCommandQueueHw<FamilyType>(context, pClDevice, 0));
    }

    template <typename FamilyType>
    void TearDownT() {
        commandQueue->release();
        kernelInternals.reset();
        EnqueueHandlerTest::TearDown();
    }

    void patchAllocateSyncBuffer() {
        sPatchAllocateSyncBuffer.SurfaceStateHeapOffset = 0;
        sPatchAllocateSyncBuffer.DataParamOffset = 0;
        sPatchAllocateSyncBuffer.DataParamSize = sizeof(uint8_t);
        kernelInternals->kernelInfo.patchInfo.pAllocateSyncBuffer = &sPatchAllocateSyncBuffer;
    }

    MockSyncBufferHandler *getSyncBufferHandler() {
        return reinterpret_cast<MockSyncBufferHandler *>(pClDevice->syncBufferHandler.get());
    }

    cl_int enqueueNDCount() {
        return clEnqueueNDCountKernelINTEL(commandQueue, kernel, workDim, gwOffset, workgroupCount, lws, 0, nullptr, nullptr);
    }

    const cl_uint workDim = 1;
    const size_t gwOffset[3] = {0, 0, 0};
    const size_t lws[3] = {10, 1, 1};
    size_t workgroupCount[3] = {10, 1, 1};
    size_t globalWorkSize[3] = {100, 1, 1};
    size_t workItemsCount = 10;
    std::unique_ptr<MockKernelWithInternals> kernelInternals;
    MockKernel *kernel;
    MockCommandQueue *commandQueue;
    SPatchAllocateSyncBuffer sPatchAllocateSyncBuffer;
};

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenAllocateSyncBufferPatchAndConcurrentKernelWhenEnqueuingKernelThenSyncBufferIsUsed) {
    patchAllocateSyncBuffer();

    enqueueNDCount();
    auto syncBufferHandler = getSyncBufferHandler();
    EXPECT_EQ(workItemsCount, syncBufferHandler->usedBufferSize);

    commandQueue->flush();
    EXPECT_EQ(syncBufferHandler->graphicsAllocation->getTaskCount(
                  pDevice->getUltCommandStreamReceiver<FamilyType>().getOsContext().getContextId()),
              pDevice->getUltCommandStreamReceiver<FamilyType>().latestSentTaskCount);
}

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenConcurrentKernelWithoutAllocateSyncBufferPatchWhenEnqueuingConcurrentKernelThenSyncBufferIsNotCreated) {
    auto retVal = enqueueNDCount();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, getSyncBufferHandler());
}

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenDefaultKernelUsingSyncBufferWhenEnqueuingKernelThenErrorIsReturnedAndSyncBufferIsNotCreated) {
    patchAllocateSyncBuffer();
    kernel->executionType = KernelExecutionType::Default;

    auto retVal = enqueueNDCount();
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
    EXPECT_EQ(nullptr, getSyncBufferHandler());
}

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenConcurrentKernelWithAllocateSyncBufferPatchWhenEnqueuingConcurrentKernelThenSyncBufferIsCreated) {
    patchAllocateSyncBuffer();
    auto retVal = enqueueNDCount();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, getSyncBufferHandler());
}

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenMaxWorkgroupCountWhenEnqueuingConcurrentKernelThenSuccessIsReturned) {
    auto maxWorkGroupCount = kernel->getMaxWorkGroupCount(workDim, lws);
    workgroupCount[0] = maxWorkGroupCount;
    globalWorkSize[0] = maxWorkGroupCount * lws[0];

    auto retVal = enqueueNDCount();
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenTooHighWorkgroupCountWhenEnqueuingConcurrentKernelThenErrorIsReturned) {
    size_t maxWorkGroupCount = kernel->getMaxWorkGroupCount(workDim, lws);
    workgroupCount[0] = maxWorkGroupCount + 1;
    globalWorkSize[0] = maxWorkGroupCount * lws[0] + 1;

    auto retVal = enqueueNDCount();
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenSyncBufferFullWhenEnqueuingKernelThenNewBufferIsAllocated) {
    patchAllocateSyncBuffer();
    enqueueNDCount();
    auto syncBufferHandler = getSyncBufferHandler();

    syncBufferHandler->usedBufferSize = syncBufferHandler->bufferSize;
    enqueueNDCount();
    EXPECT_EQ(workItemsCount, syncBufferHandler->usedBufferSize);
}

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenSshRequiredWhenPatchingSyncBufferThenSshIsProperlyPatched) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    kernelInternals->kernelInfo.usesSsh = true;
    kernelInternals->kernelInfo.requiresSshForBuffers = true;
    patchAllocateSyncBuffer();

    pClDevice->allocateSyncBufferHandler();
    auto syncBufferHandler = getSyncBufferHandler();
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(kernel->getSurfaceStateHeap(),
                                                                           sPatchAllocateSyncBuffer.SurfaceStateHeapOffset));
    auto bufferAddress = syncBufferHandler->graphicsAllocation->getGpuAddress();
    surfaceState->setSurfaceBaseAddress(bufferAddress + 1);
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();
    EXPECT_NE(bufferAddress, surfaceAddress);

    kernel->patchSyncBuffer(commandQueue->getDevice(), syncBufferHandler->graphicsAllocation, syncBufferHandler->usedBufferSize);
    surfaceAddress = surfaceState->getSurfaceBaseAddress();
    EXPECT_EQ(bufferAddress, surfaceAddress);
}

TEST(SyncBufferHandlerDeviceTest, GivenRootDeviceWhenAllocateSyncBufferIsCalledTwiceThenTheObjectIsCreatedOnlyOnce) {
    const size_t testUsedBufferSize = 100;
    MockClDevice rootDevice{new MockDevice};
    rootDevice.allocateSyncBufferHandler();
    auto syncBufferHandler = reinterpret_cast<MockSyncBufferHandler *>(rootDevice.syncBufferHandler.get());

    ASSERT_NE(syncBufferHandler->usedBufferSize, testUsedBufferSize);
    syncBufferHandler->usedBufferSize = testUsedBufferSize;

    rootDevice.allocateSyncBufferHandler();
    syncBufferHandler = reinterpret_cast<MockSyncBufferHandler *>(rootDevice.syncBufferHandler.get());

    EXPECT_EQ(testUsedBufferSize, syncBufferHandler->usedBufferSize);
}

TEST(SyncBufferHandlerDeviceTest, GivenSubDeviceWhenAllocateSyncBufferIsCalledTwiceThenTheObjectIsCreatedOnlyOnce) {
    const size_t testUsedBufferSize = 100;
    MockClDevice rootDevice{new MockDevice};
    ClDevice subDevice{*rootDevice.createSubDevice(0), platform()};
    subDevice.allocateSyncBufferHandler();
    auto syncBufferHandler = reinterpret_cast<MockSyncBufferHandler *>(subDevice.syncBufferHandler.get());

    ASSERT_NE(syncBufferHandler->usedBufferSize, testUsedBufferSize);
    syncBufferHandler->usedBufferSize = testUsedBufferSize;

    subDevice.allocateSyncBufferHandler();
    syncBufferHandler = reinterpret_cast<MockSyncBufferHandler *>(subDevice.syncBufferHandler.get());

    EXPECT_EQ(testUsedBufferSize, syncBufferHandler->usedBufferSize);
}

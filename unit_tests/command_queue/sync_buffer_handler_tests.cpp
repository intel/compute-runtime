/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/program/sync_buffer_handler.h"
#include "runtime/api/api.h"
#include "test.h"
#include "unit_tests/fixtures/enqueue_handler_fixture.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"

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
        kernelInternals = std::make_unique<MockKernelWithInternals>(*pDevice, context);
        kernel = kernelInternals->mockKernel;
        commandQueue = reinterpret_cast<MockCommandQueue *>(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));
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
        return reinterpret_cast<MockSyncBufferHandler *>(pDevice->syncBufferHandler.get());
    }

    const cl_uint workDim = 1;
    const size_t gwOffset[3] = {0, 0, 0};
    const size_t lws[3] = {10, 1, 1};
    size_t workgroupCount[3] = {10, 1, 1};
    size_t workItemsCount = 10;
    std::unique_ptr<MockKernelWithInternals> kernelInternals;
    MockKernel *kernel;
    MockCommandQueue *commandQueue;
    SPatchAllocateSyncBuffer sPatchAllocateSyncBuffer;
};

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenAllocateSyncBufferPatchWhenEnqueuingKernelThenSyncBufferIsUsed) {
    patchAllocateSyncBuffer();
    clEnqueueNDRangeKernelINTEL(commandQueue, kernel, workDim, gwOffset, workgroupCount, lws, 0, nullptr, nullptr);

    auto syncBufferHandler = getSyncBufferHandler();
    EXPECT_EQ(workItemsCount, syncBufferHandler->usedBufferSize);

    commandQueue->flush();
    EXPECT_EQ(syncBufferHandler->graphicsAllocation->getTaskCount(
                  pDevice->getUltCommandStreamReceiver<FamilyType>().getOsContext().getContextId()),
              pDevice->getUltCommandStreamReceiver<FamilyType>().latestSentTaskCount);
}

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenNoAllocateSyncBufferPatchWhenEnqueuingKernelThenSyncBufferIsNotUsedAndUsedBufferSizeIsNotUpdated) {
    clEnqueueNDRangeKernelINTEL(commandQueue, kernel, workDim, gwOffset, workgroupCount, lws, 0, nullptr, nullptr);

    auto syncBufferHandler = getSyncBufferHandler();
    EXPECT_EQ(0u, syncBufferHandler->usedBufferSize);
}

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenMaxWorkgroupCountWhenEnqueuingKernelThenSuccessIsReturned) {
    auto maxWorkGroupCount = kernel->getMaxWorkGroupCount(workDim, lws);
    workgroupCount[0] = maxWorkGroupCount;
    auto retVal = clEnqueueNDRangeKernelINTEL(commandQueue, kernel, workDim, gwOffset, workgroupCount, lws, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenTooHighWorkgroupCountWhenEnqueuingKernelThenErrorIsReturned) {
    size_t maxWorkGroupCount = kernel->getMaxWorkGroupCount(workDim, lws);
    workgroupCount[0] = maxWorkGroupCount + 1;
    auto retVal = clEnqueueNDRangeKernelINTEL(commandQueue, kernel, workDim, gwOffset, workgroupCount, lws, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenSyncBufferFullWhenEnqueuingKernelThenNewBufferIsAllocated) {
    patchAllocateSyncBuffer();
    clEnqueueNDRangeKernelINTEL(commandQueue, kernel, workDim, gwOffset, workgroupCount, lws, 0, nullptr, nullptr);

    auto syncBufferHandler = getSyncBufferHandler();
    syncBufferHandler->usedBufferSize = syncBufferHandler->bufferSize;
    clEnqueueNDRangeKernelINTEL(commandQueue, kernel, workDim, gwOffset, workgroupCount, lws, 0, nullptr, nullptr);
    EXPECT_EQ(workItemsCount, syncBufferHandler->usedBufferSize);
}

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenSshRequiredWhenPatchingSyncBufferThenSshIsProperlyPatched) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    kernelInternals->kernelInfo.usesSsh = true;
    kernelInternals->kernelInfo.requiresSshForBuffers = true;
    patchAllocateSyncBuffer();

    pDevice->allocateSyncBufferHandler();
    auto syncBufferHandler = getSyncBufferHandler();
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(ptrOffset(kernel->getSurfaceStateHeap(),
                                                                                 sPatchAllocateSyncBuffer.SurfaceStateHeapOffset));
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();
    auto bufferAddress = syncBufferHandler->graphicsAllocation->getGpuAddress();
    EXPECT_NE(bufferAddress, surfaceAddress);

    kernel->patchSyncBuffer(commandQueue->getDevice(), syncBufferHandler->graphicsAllocation, syncBufferHandler->usedBufferSize);
    surfaceAddress = surfaceState->getSurfaceBaseAddress();
    EXPECT_EQ(bufferAddress, surfaceAddress);
}

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenKernelUsingSyncBufferWhenUsingStandardEnqueueThenErrorIsReturned) {
    patchAllocateSyncBuffer();

    size_t globalWorkSize[3] = {workgroupCount[0] * lws[0], workgroupCount[1] * lws[1], workgroupCount[2] * lws[2]};
    auto retVal = clEnqueueNDRangeKernel(commandQueue, kernel, workDim, gwOffset, globalWorkSize, lws, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST(SyncBufferHandlerDeviceTest, GivenRootDeviceWhenAllocateSyncBufferIsCalledTwiceThenTheObjectIsCreatedOnlyOnce) {
    const size_t testUsedBufferSize = 100;
    MockDevice rootDevice;
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
    MockDevice rootDevice;
    std::unique_ptr<MockSubDevice> subDevice{reinterpret_cast<MockSubDevice *>(rootDevice.createSubDevice(0))};
    subDevice->allocateSyncBufferHandler();
    auto syncBufferHandler = reinterpret_cast<MockSyncBufferHandler *>(subDevice->syncBufferHandler.get());

    ASSERT_NE(syncBufferHandler->usedBufferSize, testUsedBufferSize);
    syncBufferHandler->usedBufferSize = testUsedBufferSize;

    subDevice->allocateSyncBufferHandler();
    syncBufferHandler = reinterpret_cast<MockSyncBufferHandler *>(subDevice->syncBufferHandler.get());

    EXPECT_EQ(testUsedBufferSize, syncBufferHandler->usedBufferSize);
}

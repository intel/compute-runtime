/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/mocks/mock_sync_buffer_handler.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/api/api.h"
#include "opencl/test/unit_test/fixtures/enqueue_handler_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "aubstream/engine_node.h"

namespace NEO {
template <typename GfxFamily>
class UltCommandStreamReceiver;
} // namespace NEO

using namespace NEO;

class SyncBufferEnqueueHandlerTest : public EnqueueHandlerTest {
  public:
    void SetUp() override {
        hardwareInfo = *defaultHwInfo;
        hardwareInfo.capabilityTable.blitterOperationsSupported = true;
        auto releaseHelper = ReleaseHelper::create(hardwareInfo.ipVersion);
        hardwareInfoSetup[productFamily](&hardwareInfo, true, 0, releaseHelper.get());
        setUpImpl(&hardwareInfo);
    }

    void TearDown() override {
        context->decRefInternal();
        delete pClDevice;
        pClDevice = nullptr;
        pDevice = nullptr;
    }

    void setUpImpl(const NEO::HardwareInfo *hardwareInfo) {
        pDevice = MockDevice::createWithNewExecutionEnvironment<MockDevice>(hardwareInfo);
        ASSERT_NE(nullptr, pDevice);
        pClDevice = new MockClDevice{pDevice};
        ASSERT_NE(nullptr, pClDevice);

        auto &commandStreamReceiver = pDevice->getGpgpuCommandStreamReceiver();
        pTagMemory = commandStreamReceiver.getTagAddress();
        ASSERT_NE(nullptr, const_cast<TagAddressType *>(pTagMemory));

        context = new NEO::MockContext(pClDevice);
    }
};

class SyncBufferHandlerTest : public SyncBufferEnqueueHandlerTest {
  public:
    void SetUp() override {}
    void TearDown() override {}

    template <typename FamilyType>
    void setUpT() {
        SyncBufferEnqueueHandlerTest::SetUp();
        kernelInternals = std::make_unique<MockKernelWithInternals>(*pClDevice, context);
        kernelInternals->kernelInfo.kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Stateless;
        kernel = kernelInternals->mockKernel;
        kernel->executionType = KernelExecutionType::concurrent;
        commandQueue = reinterpret_cast<MockCommandQueue *>(new MockCommandQueueHw<FamilyType>(context, pClDevice, 0));
        auto &hwInfo = pClDevice->getHardwareInfo();
        auto &productHelper = pClDevice->getProductHelper();
        if (productHelper.isCooperativeEngineSupported(hwInfo)) {
            auto engine = pClDevice->device.tryGetEngine(aub_stream::EngineType::ENGINE_CCS, EngineUsage::cooperative);
            if (engine) {
                commandQueue->gpgpuEngine = engine;
            } else {
                GTEST_SKIP();
            }
        }
    }

    template <typename FamilyType>
    void tearDownT() {
        commandQueue->release();
        kernelInternals.reset();
        SyncBufferEnqueueHandlerTest::TearDown();
    }

    void patchAllocateSyncBuffer() {
        kernelInternals->kernelInfo.setSyncBuffer(sizeof(uint32_t), 0, 0);
    }

    MockSyncBufferHandler *getSyncBufferHandler() {
        return reinterpret_cast<MockSyncBufferHandler *>(pDevice->syncBufferHandler.get());
    }

    cl_int enqueueNDCount() {
        return clEnqueueNDCountKernelINTEL(commandQueue, kernelInternals->mockMultiDeviceKernel, workDim, gwOffset, workgroupCount, lws, 0, nullptr, nullptr);
    }

    bool isCooperativeDispatchSupported() {
        auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
        auto engineGroupType = gfxCoreHelper.getEngineGroupType(commandQueue->getGpgpuEngine().getEngineType(),
                                                                commandQueue->getGpgpuEngine().getEngineUsage(), hardwareInfo);
        return gfxCoreHelper.isCooperativeDispatchSupported(engineGroupType, pDevice->getRootDeviceEnvironment());
    }

    const cl_uint workDim = 1;
    const size_t gwOffset[3] = {0, 0, 0};
    const size_t workItemsCount = 16;
    const size_t lws[3] = {workItemsCount, 1, 1};
    size_t workgroupCount[3] = {workItemsCount, 1, 1};
    std::unique_ptr<MockKernelWithInternals> kernelInternals;
    MockKernel *kernel;
    MockCommandQueue *commandQueue;
};

HWTEST2_TEMPLATED_F(SyncBufferHandlerTest, GivenAllocateSyncBufferPatchAndConcurrentKernelWhenEnqueuingKernelThenSyncBufferIsUsed, HasDispatchAllSupport) {
    patchAllocateSyncBuffer();

    enqueueNDCount();
    auto syncBufferHandler = getSyncBufferHandler();
    EXPECT_EQ(workItemsCount, syncBufferHandler->usedBufferSize);

    commandQueue->flush();

    auto pCsr = commandQueue->getGpgpuEngine().commandStreamReceiver;
    EXPECT_EQ(syncBufferHandler->graphicsAllocation->getTaskCount(pCsr->getOsContext().getContextId()),
              static_cast<UltCommandStreamReceiver<FamilyType> *>(pCsr)->latestSentTaskCount);
}

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenAllocateSyncBufferPatchAndConcurrentKernelWhenEnqueuingKernelThenSyncBufferOffsetIsProperlyAligned) {
    patchAllocateSyncBuffer();

    workgroupCount[0] = 1;
    enqueueNDCount();

    auto syncBufferHandler = getSyncBufferHandler();
    auto minimalSyncBufferSize = alignUp(CommonConstants::minimalSyncBufferSize, CommonConstants::maximalSizeOfAtomicType);
    EXPECT_EQ(minimalSyncBufferSize, syncBufferHandler->usedBufferSize);

    enqueueNDCount();
    EXPECT_EQ(2u * minimalSyncBufferSize, syncBufferHandler->usedBufferSize);
}

HWTEST2_TEMPLATED_F(SyncBufferHandlerTest, GivenConcurrentKernelWithoutAllocateSyncBufferPatchWhenEnqueuingConcurrentKernelThenSyncBufferIsNotCreated, HasDispatchAllSupport) {
    auto retVal = enqueueNDCount();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, getSyncBufferHandler());
}

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenDefaultKernelUsingSyncBufferWhenEnqueuingKernelThenErrorIsReturnedAndSyncBufferIsNotCreated) {
    patchAllocateSyncBuffer();
    kernel->executionType = KernelExecutionType::defaultType;

    auto retVal = enqueueNDCount();
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
    EXPECT_EQ(nullptr, getSyncBufferHandler());
}

HWTEST2_TEMPLATED_F(SyncBufferHandlerTest, GivenConcurrentKernelWithAllocateSyncBufferPatchWhenEnqueuingConcurrentKernelThenSyncBufferIsCreated, HasDispatchAllSupport) {
    patchAllocateSyncBuffer();
    auto retVal = enqueueNDCount();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, getSyncBufferHandler());
}

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenMaxWorkgroupCountWhenEnqueuingConcurrentKernelThenSuccessIsReturned) {
    auto maxWorkGroupCount = kernel->getMaxWorkGroupCount(workDim, lws, commandQueue, false);
    workgroupCount[0] = maxWorkGroupCount;

    auto retVal = enqueueNDCount();
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenTooHighWorkgroupCountWhenEnqueuingConcurrentKernelThenErrorIsReturned) {
    size_t maxWorkGroupCount = kernel->getMaxWorkGroupCount(workDim, lws, commandQueue, false);
    workgroupCount[0] = maxWorkGroupCount + 1;

    auto retVal = enqueueNDCount();
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

HWTEST2_TEMPLATED_F(SyncBufferHandlerTest, GivenSyncBufferFullWhenEnqueuingKernelThenNewBufferIsAllocated, HasDispatchAllSupport) {
    patchAllocateSyncBuffer();
    enqueueNDCount();
    auto syncBufferHandler = getSyncBufferHandler();

    syncBufferHandler->usedBufferSize = syncBufferHandler->bufferSize;
    enqueueNDCount();
    EXPECT_EQ(workItemsCount, syncBufferHandler->usedBufferSize);
}

HWTEST_TEMPLATED_F(SyncBufferHandlerTest, GivenSshRequiredWhenPatchingSyncBufferThenSshIsProperlyPatched) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    kernelInternals->kernelInfo.setBufferAddressingMode(KernelDescriptor::BindfulAndStateless);

    patchAllocateSyncBuffer();

    pDevice->allocateSyncBufferHandler();
    auto syncBufferHandler = getSyncBufferHandler();
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(kernel->getSurfaceStateHeap(),
                                                                           kernel->getKernelInfo().kernelDescriptor.payloadMappings.implicitArgs.syncBufferAddress.bindful));
    auto bufferAddress = syncBufferHandler->graphicsAllocation->getGpuAddress();
    surfaceState->setSurfaceBaseAddress(bufferAddress + 1);
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();
    EXPECT_NE(bufferAddress, surfaceAddress);

    kernel->patchSyncBuffer(syncBufferHandler->graphicsAllocation, syncBufferHandler->usedBufferSize);
    surfaceAddress = surfaceState->getSurfaceBaseAddress();
    EXPECT_EQ(bufferAddress, surfaceAddress);
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
    UltDeviceFactory ultDeviceFactory{1, 2};
    auto pSubDevice = ultDeviceFactory.subDevices[0];
    pSubDevice->allocateSyncBufferHandler();
    auto syncBufferHandler = reinterpret_cast<MockSyncBufferHandler *>(pSubDevice->syncBufferHandler.get());

    const size_t testUsedBufferSize = 100;
    ASSERT_NE(syncBufferHandler->usedBufferSize, testUsedBufferSize);
    syncBufferHandler->usedBufferSize = testUsedBufferSize;

    pSubDevice->allocateSyncBufferHandler();
    syncBufferHandler = reinterpret_cast<MockSyncBufferHandler *>(pSubDevice->syncBufferHandler.get());

    EXPECT_EQ(testUsedBufferSize, syncBufferHandler->usedBufferSize);
}

TEST(SyncBufferHandlerDeviceTest, givenMultipleSubDevicesWhenAllocatingSyncBufferThenClonePageTables) {
    UltDeviceFactory ultDeviceFactory{1, 2};
    auto rootDevice = ultDeviceFactory.rootDevices[0];
    rootDevice->allocateSyncBufferHandler();
    auto syncBufferHandler = reinterpret_cast<MockSyncBufferHandler *>(rootDevice->syncBufferHandler.get());

    EXPECT_TRUE(syncBufferHandler->graphicsAllocation->storageInfo.cloningOfPageTables);
}

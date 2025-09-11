/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/helpers/dispatch_info_builder.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_builder.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_cl_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"

using namespace NEO;

using MultiIoqCmdQSynchronizationTest = CommandQueueHwBlitTest<false>;

HWTEST_F(MultiIoqCmdQSynchronizationTest, givenTwoIoqCmdQsWhenEnqueuesSynchronizedWithMarkersThenCorrectSynchronizationIsApplied) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    if (pCmdQ->getTimestampPacketContainer() == nullptr) {
        GTEST_SKIP();
    }

    auto buffer = std::unique_ptr<Buffer>{BufferHelper<>::create(pContext)};
    char ptr[1] = {};

    auto pCmdQ2 = createCommandQueue(pClDevice);
    cl_event outEvent;
    size_t offset = 0;
    cl_int status;
    status = pCmdQ->enqueueWriteBuffer(buffer.get(), CL_FALSE, offset, 1u, ptr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    status = pCmdQ->enqueueMarkerWithWaitList(0, nullptr, &outEvent);
    EXPECT_EQ(CL_SUCCESS, status);

    auto node = castToObject<Event>(outEvent)->getTimestampPacketNodes()->peekNodes().at(0);
    const auto nodeGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*node);

    auto cmdQ2Start = pCmdQ2->getCS(0).getUsed();
    status = pCmdQ2->enqueueMarkerWithWaitList(1, &outEvent, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    auto bcsStart = pCmdQ2->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->getCS(0).getUsed();
    status = pCmdQ2->enqueueReadBuffer(buffer.get(), CL_FALSE, offset, 1u, ptr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    uint64_t bcsSemaphoreAddress = 0x0;

    {
        LinearStream &bcsStream = pCmdQ2->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->getCS(0);
        HardwareParse bcsHwParser;
        bcsHwParser.parseCommands<FamilyType>(bcsStream, bcsStart);
        auto semaphoreCmdBcs = genCmdCast<MI_SEMAPHORE_WAIT *>(*bcsHwParser.cmdList.begin());
        EXPECT_NE(nullptr, semaphoreCmdBcs);
        EXPECT_EQ(1u, semaphoreCmdBcs->getSemaphoreDataDword());
        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, semaphoreCmdBcs->getCompareOperation());
        bcsSemaphoreAddress = semaphoreCmdBcs->getSemaphoreGraphicsAddress();
    }

    {
        LinearStream &cmdQ2Stream = pCmdQ2->getCS(0);
        HardwareParse ccsHwParser;
        ccsHwParser.parseCommands<FamilyType>(cmdQ2Stream, cmdQ2Start);
        const auto semaphoreCcsItor = find<MI_SEMAPHORE_WAIT *>(ccsHwParser.cmdList.begin(), ccsHwParser.cmdList.end());
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreCcsItor);
        ASSERT_NE(nullptr, semaphoreCmd);
        EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(nodeGpuAddress, semaphoreCmd->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, semaphoreCmd->getCompareOperation());

        bool pipeControlForBcsSemaphoreFound = false;
        auto pipeControlsAfterSemaphore = findAll<PIPE_CONTROL *>(semaphoreCcsItor, ccsHwParser.cmdList.end());
        for (auto pipeControlIter : pipeControlsAfterSemaphore) {
            auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*pipeControlIter);
            if (0u == pipeControlCmd->getImmediateData() &&
                PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA == pipeControlCmd->getPostSyncOperation() &&
                NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd) == bcsSemaphoreAddress) {
                pipeControlForBcsSemaphoreFound = true;
                break;
            }
        }
        EXPECT_TRUE(pipeControlForBcsSemaphoreFound);
    }

    EXPECT_EQ(CL_SUCCESS, pCmdQ->finish(false));
    EXPECT_EQ(CL_SUCCESS, pCmdQ2->finish(false));

    clReleaseEvent(outEvent);
    // tearDown
    if (pCmdQ2) {
        auto blocked = pCmdQ2->isQueueBlocked();
        UNRECOVERABLE_IF(blocked);
        pCmdQ2->release();
    }
}

struct BuiltinParamsCommandQueueHwTests : public CommandQueueHwTest {

    void setUpImpl(EBuiltInOps::Type operation) {
        auto builtIns = new MockBuiltins();
        MockRootDeviceEnvironment::resetBuiltins(pCmdQ->getDevice().getExecutionEnvironment()->rootDeviceEnvironments[pCmdQ->getDevice().getRootDeviceIndex()].get(), builtIns);

        auto swapBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
            rootDeviceIndex,
            operation,
            std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuilder(*builtIns, pCmdQ->getClDevice())));

        mockBuilder = static_cast<MockBuilder *>(&BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
            operation,
            *pClDevice));
    }

    MockBuilder *mockBuilder;
};

HWTEST_F(BuiltinParamsCommandQueueHwTests, givenEnqueueReadWriteBufferCallWhenBuiltinParamsArePassedThenCheckValuesCorectness) {

    auto builtInType = EBuiltInOps::copyBufferToBuffer;

    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    if (compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo)) {
        builtInType = EBuiltInOps::copyBufferToBufferStatelessHeapless;
    } else if (compilerProductHelper.isForceToStatelessRequired()) {
        builtInType = EBuiltInOps::copyBufferToBufferStateless;
    }

    setUpImpl(builtInType);
    BufferDefaults::context = context;
    auto buffer = clUniquePtr(BufferHelper<>::create());

    char array[3 * MemoryConstants::cacheLineSize];
    char *ptr = &array[MemoryConstants::cacheLineSize];
    ptr = alignUp(ptr, MemoryConstants::cacheLineSize);
    ptr -= 1;

    cl_int status = pCmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 0, ptr, nullptr, 0, 0, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    void *alignedPtr = alignDown(ptr, 4);
    size_t ptrOffset = ptrDiff(ptr, alignedPtr);
    Vec3<size_t> offset = {0, 0, 0};

    auto builtinParams = mockBuilder->paramsReceived.multiDispatchInfo.peekBuiltinOpParams();

    EXPECT_EQ(alignedPtr, builtinParams.dstPtr);
    EXPECT_EQ(ptrOffset, builtinParams.dstOffset.x);
    EXPECT_EQ(offset, builtinParams.srcOffset);

    status = pCmdQ->enqueueWriteBuffer(buffer.get(), CL_FALSE, 0, 0, ptr, nullptr, 0, 0, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    builtinParams = mockBuilder->paramsReceived.multiDispatchInfo.peekBuiltinOpParams();

    EXPECT_EQ(alignedPtr, builtinParams.srcPtr);
    EXPECT_EQ(ptrOffset, builtinParams.srcOffset.x);
    EXPECT_EQ(offset, builtinParams.dstOffset);
}

HWTEST_F(BuiltinParamsCommandQueueHwTests, givenEnqueueWriteImageCallWhenBuiltinParamsArePassedThenCheckValuesCorectness) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCopyWithStagingBuffers.set(0);

    bool heaplessAllowed = UnitTestHelper<FamilyType>::isHeaplessAllowed();
    const bool useStateless = pDevice->getCompilerProductHelper().isForceToStatelessRequired();

    for (auto useHeapless : {false, heaplessAllowed}) {
        if (useHeapless && !heaplessAllowed) {
            continue;
        }

        reinterpret_cast<MockCommandQueueHw<FamilyType> *>(pCmdQ)->heaplessModeEnabled = useHeapless;
        setUpImpl(EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferToImage3d>(useStateless, useHeapless));

        std::unique_ptr<Image> dstImage(ImageHelperUlt<ImageUseHostPtr<Image2dDefaults>>::create(context));

        auto imageDesc = dstImage->getImageDesc();
        size_t origin[] = {0, 0, 0};
        size_t region[] = {imageDesc.image_width, imageDesc.image_height, 0};

        size_t rowPitch = dstImage->getHostPtrRowPitch();
        size_t slicePitch = dstImage->getHostPtrSlicePitch();

        char array[3 * MemoryConstants::cacheLineSize];
        char *ptr = &array[MemoryConstants::cacheLineSize];
        ptr = alignUp(ptr, MemoryConstants::cacheLineSize);
        ptr -= 1;

        void *alignedPtr = alignDown(ptr, 4);
        size_t ptrOffset = ptrDiff(ptr, alignedPtr);
        Vec3<size_t> offset = {0, 0, 0};

        cl_int status = pCmdQ->enqueueWriteImage(dstImage.get(),
                                                 CL_FALSE,
                                                 origin,
                                                 region,
                                                 rowPitch,
                                                 slicePitch,
                                                 ptr,
                                                 nullptr,
                                                 0,
                                                 0,
                                                 nullptr);
        EXPECT_EQ(CL_SUCCESS, status);

        auto builtinParams = mockBuilder->paramsReceived.multiDispatchInfo.peekBuiltinOpParams();
        EXPECT_EQ(alignedPtr, builtinParams.srcPtr);
        EXPECT_EQ(ptrOffset, builtinParams.srcOffset.x);
        EXPECT_EQ(offset, builtinParams.dstOffset);
    }
}

HWTEST_F(BuiltinParamsCommandQueueHwTests, givenEnqueueReadImageCallWhenBuiltinParamsArePassedThenCheckValuesCorectness) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    const bool useStateless = pDevice->getCompilerProductHelper().isForceToStatelessRequired();
    setUpImpl(EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyImage3dToBuffer>(useStateless, pCmdQ->getHeaplessModeEnabled()));

    std::unique_ptr<Image> dstImage(ImageHelperUlt<ImageUseHostPtr<Image2dDefaults>>::create(context));

    auto imageDesc = dstImage->getImageDesc();
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, 0};

    size_t rowPitch = dstImage->getHostPtrRowPitch();
    size_t slicePitch = dstImage->getHostPtrSlicePitch();

    char array[3 * MemoryConstants::cacheLineSize];
    char *ptr = &array[MemoryConstants::cacheLineSize];
    ptr = alignUp(ptr, MemoryConstants::cacheLineSize);
    ptr -= 1;

    void *alignedPtr = alignDown(ptr, 4);
    size_t ptrOffset = ptrDiff(ptr, alignedPtr);
    Vec3<size_t> offset = {0, 0, 0};

    cl_int status = pCmdQ->enqueueReadImage(dstImage.get(),
                                            CL_FALSE,
                                            origin,
                                            region,
                                            rowPitch,
                                            slicePitch,
                                            ptr,
                                            nullptr,
                                            0,
                                            0,
                                            nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    auto builtinParams = mockBuilder->paramsReceived.multiDispatchInfo.peekBuiltinOpParams();
    EXPECT_EQ(alignedPtr, builtinParams.dstPtr);
    EXPECT_EQ(ptrOffset, builtinParams.dstOffset.x);
    EXPECT_EQ(offset, builtinParams.srcOffset);
}

HWTEST_F(BuiltinParamsCommandQueueHwTests, givenEnqueueReadWriteBufferRectCallWhenBuiltinParamsArePassedThenCheckValuesCorectness) {

    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    auto builtIn = EBuiltInOps::copyBufferRect;
    if (compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo)) {
        builtIn = EBuiltInOps::copyBufferRectStatelessHeapless;
    } else if (pCmdQ->getDevice().getCompilerProductHelper().isForceToStatelessRequired()) {
        builtIn = EBuiltInOps::copyBufferRectStateless;
    }

    setUpImpl(builtIn);

    BufferDefaults::context = context;
    auto buffer = clUniquePtr(BufferHelper<>::create());

    size_t bufferOrigin[3] = {0, 0, 0};
    size_t hostOrigin[3] = {0, 0, 0};
    size_t region[3] = {0, 0, 0};

    char array[3 * MemoryConstants::cacheLineSize];
    char *ptr = &array[MemoryConstants::cacheLineSize];
    ptr = alignUp(ptr, MemoryConstants::cacheLineSize);
    ptr -= 1;

    cl_int status = pCmdQ->enqueueReadBufferRect(buffer.get(), CL_FALSE, bufferOrigin, hostOrigin, region, 0, 0, 0, 0, ptr, 0, 0, nullptr);

    void *alignedPtr = alignDown(ptr, 4);
    size_t ptrOffset = ptrDiff(ptr, alignedPtr);
    Vec3<size_t> offset = {0, 0, 0};
    auto builtinParams = mockBuilder->paramsReceived.multiDispatchInfo.peekBuiltinOpParams();

    EXPECT_EQ(alignedPtr, builtinParams.dstPtr);
    EXPECT_EQ(ptrOffset, builtinParams.dstOffset.x);
    EXPECT_EQ(offset, builtinParams.srcOffset);

    status = pCmdQ->enqueueWriteBufferRect(buffer.get(), CL_FALSE, bufferOrigin, hostOrigin, region, 0, 0, 0, 0, ptr, 0, 0, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    builtinParams = mockBuilder->paramsReceived.multiDispatchInfo.peekBuiltinOpParams();
    EXPECT_EQ(alignedPtr, builtinParams.srcPtr);
    EXPECT_EQ(offset, builtinParams.dstOffset);
    EXPECT_EQ(ptrOffset, builtinParams.srcOffset.x);
}

struct OOQueueHwTestWithMockCsr : public OOQueueHwTest {
    void SetUp() override {}
    void TearDown() override {}

    template <typename FamilyType>
    void setUpT() {
        EnvironmentWithCsrWrapper environment;
        environment.setCsrType<MockCsr<FamilyType>>();
        OOQueueHwTest::SetUp();
    }

    template <typename FamilyType>
    void tearDownT() {
        OOQueueHwTest::TearDown();
    }
};

HWTEST_TEMPLATED_F(OOQueueHwTestWithMockCsr, givenBlockedOutOfOrderCmdQueueAndAsynchronouslyCompletedEventWhenEnqueueCompletesVirtualEventThenUpdatedTaskLevelIsPassedToEnqueueAndFlushTask) {
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);

    auto mockCSR = static_cast<MockCsr<FamilyType> *>(&pDevice->getUltCommandStreamReceiver<FamilyType>());

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;
    size_t offset = 0;
    size_t size = 1;

    class MockEventWithSetCompleteOnUpdate : public Event {
      public:
        MockEventWithSetCompleteOnUpdate(CommandQueue *cmdQueue, cl_command_type cmdType,
                                         uint32_t taskLevel, TaskCountType taskCount) : Event(cmdQueue, cmdType, taskLevel, taskCount) {
        }
        void updateExecutionStatus() override {
            setStatus(CL_COMPLETE);
        }
    };

    Event event(cmdQHw, CL_COMMAND_NDRANGE_KERNEL, 10, 0);

    uint32_t virtualEventTaskLevel = 77;
    uint32_t virtualEventTaskCount = 80;
    MockEventWithSetCompleteOnUpdate virtualEvent(cmdQHw, CL_COMMAND_NDRANGE_KERNEL, virtualEventTaskLevel, virtualEventTaskCount);

    cl_event blockedEvent = &event;

    // Put Queue in blocked state by assigning virtualEvent
    virtualEvent.incRefInternal();
    event.addChild(virtualEvent);
    cmdQHw->virtualEvent = &virtualEvent;

    cmdQHw->taskLevel = 23;
    cmdQHw->enqueueKernel(mockKernel, 1, &offset, &size, &size, 1, &blockedEvent, nullptr);
    // new virtual event is created on enqueue, bind it to the created virtual event
    EXPECT_NE(cmdQHw->virtualEvent, &virtualEvent);

    event.setStatus(CL_SUBMITTED);

    virtualEvent.Event::updateExecutionStatus();
    EXPECT_FALSE(cmdQHw->isQueueBlocked());

    //+1 due to dependency between virtual event & new virtual event
    // new virtual event is actually responsible for command delivery
    EXPECT_EQ(virtualEventTaskLevel + 1, cmdQHw->taskLevel);
    EXPECT_EQ(virtualEventTaskLevel + 1, mockCSR->lastTaskLevelToFlushTask);
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenCheckIsSplitEnqueueBlitSupportedThenReturnProperValue) {
    DebugManagerStateRestore restorer;
    auto *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);
    {
        debugManager.flags.SplitBcsCopy.set(1);
        EXPECT_TRUE(cmdQHw->getDevice().isBcsSplitSupported());
    }
    {
        debugManager.flags.SplitBcsCopy.set(0);
        EXPECT_FALSE(cmdQHw->getDevice().isBcsSplitSupported());
    }
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenCheckIsSplitEnqueueBlitNeededThenReturnProperValue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    auto *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    ultHwConfig.useBlitSplit = true;
    {
        EXPECT_TRUE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::hostToHost, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::localToLocal, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_TRUE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::localToHost, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_TRUE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::hostToLocal, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
    }
    {
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::hostToHost, 64 * MemoryConstants::megaByte, cmdQHw->getGpgpuCommandStreamReceiver()));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::localToLocal, 64 * MemoryConstants::megaByte, cmdQHw->getGpgpuCommandStreamReceiver()));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::localToHost, 64 * MemoryConstants::megaByte, cmdQHw->getGpgpuCommandStreamReceiver()));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::hostToLocal, 64 * MemoryConstants::megaByte, cmdQHw->getGpgpuCommandStreamReceiver()));
    }
    {
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::hostToHost, 4 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::localToLocal, 4 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::localToHost, 4 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::hostToLocal, 4 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
    }
    {
        debugManager.flags.SplitBcsCopy.set(0);
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::hostToHost, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::localToLocal, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::localToHost, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::hostToLocal, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
    }
    {
        debugManager.flags.SplitBcsCopy.set(-1);
        ultHwConfig.useBlitSplit = false;
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::hostToHost, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::localToLocal, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::localToHost, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::hostToLocal, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
    }
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenSplitBcsSizeSetWhenCheckIsSplitEnqueueBlitNeededThenReturnProperValue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    debugManager.flags.SplitBcsSize.set(100);
    auto *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    ultHwConfig.useBlitSplit = true;
    {
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::hostToLocal, 150 * MemoryConstants::kiloByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
    }
    {
        MockCommandQueueHw<FamilyType> queue(this->pContext, this->pClDevice, nullptr);
        EXPECT_TRUE(queue.isSplitEnqueueBlitNeeded(TransferDirection::hostToLocal, 150 * MemoryConstants::kiloByte, *queue.getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_EQ(queue.minimalSizeForBcsSplit, 100 * MemoryConstants::kiloByte);
    }
}

char hostPtr[16 * MemoryConstants::megaByte];
struct BcsSplitBufferTraits {
    enum { flags = CL_MEM_READ_WRITE };
    static size_t sizeInBytes;
    static void *hostPtr;
    static NEO::Context *context;
};

void *BcsSplitBufferTraits::hostPtr = static_cast<void *>(&hostPtr);
size_t BcsSplitBufferTraits::sizeInBytes = 16 * MemoryConstants::megaByte;
Context *BcsSplitBufferTraits::context = nullptr;

HWTEST_F(IoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenEnqueueReadThenEnqueueBlitSplit) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    debugManager.flags.SplitBcsMaskD2H.set(0b1010);
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;

    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));

    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());
    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    auto cmdQHw = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    const auto gpgpuTaskCountBefore = cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount();
    const auto bcsTaskCountBefore = cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount();

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::localMemory;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(csr1->peekTaskCount(), 1u);
    EXPECT_EQ(csr2->peekTaskCount(), 1u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    EXPECT_EQ(cmdQHw->kernelParams.size.x, 8 * MemoryConstants::megaByte);

    const_cast<StackVec<TagNodeBase *, 32u> &>(cmdQHw->timestampPacketContainer->peekNodes()).clear();
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenSplitBcsCopyWithEventWhenEnqueueReadThenEnableProfiling) {
    class MyCmdQueue : public MockCommandQueueHw<FamilyType> {
      public:
        using BaseClass = MockCommandQueueHw<FamilyType>;

        using BaseClass::MockCommandQueueHw;

        BlitProperties processDispatchForBlitEnqueue(CommandStreamReceiver &blitCommandStreamReceiver,
                                                     const MultiDispatchInfo &multiDispatchInfo,
                                                     TimestampPacketDependencies &timestampPacketDependencies,
                                                     const EventsRequest &eventsRequest,
                                                     LinearStream *commandStream,
                                                     uint32_t commandType, bool queueBlocked, bool profilingEnabled, TagNodeBase *multiRootDeviceEventSync) override {
            profilingDispatch = true;
            return BaseClass::processDispatchForBlitEnqueue(blitCommandStreamReceiver, multiDispatchInfo, timestampPacketDependencies, eventsRequest, commandStream, commandType, queueBlocked,
                                                            profilingEnabled, multiRootDeviceEventSync);
        }

        bool profilingDispatch = false;
    };

    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    debugManager.flags.SplitBcsMaskD2H.set(0b1010);
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;

    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));

    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());
    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    auto cmdQHw = std::make_unique<MyCmdQueue>(context, pClDevice, nullptr);
    cmdQHw->setProfilingEnabled();

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    const auto gpgpuTaskCountBefore = cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount();
    const auto bcsTaskCountBefore = cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount();

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::localMemory;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    cl_event event = nullptr;
    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, &event));

    EXPECT_EQ(csr1->peekTaskCount(), 1u);
    EXPECT_EQ(csr2->peekTaskCount(), 1u);

    EXPECT_TRUE(cmdQHw->profilingDispatch);

    const_cast<StackVec<TagNodeBase *, 32u> &>(cmdQHw->timestampPacketContainer->peekNodes()).clear();

    clReleaseEvent(event);
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenSplitBcsCopyAndD2HMaskWhenEnqueueReadThenEnqueueBlitSplit) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    debugManager.flags.SplitBcsMaskD2H.set(0b10);
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;

    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));

    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());
    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    auto cmdQHw = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    const auto gpgpuTaskCountBefore = cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount();
    const auto bcsTaskCountBefore = cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount();

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::localMemory;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(csr1->peekTaskCount(), 1u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    EXPECT_EQ(cmdQHw->kernelParams.size.x, 16 * MemoryConstants::megaByte);

    const_cast<StackVec<TagNodeBase *, 32u> &>(cmdQHw->timestampPacketContainer->peekNodes()).clear();
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenSplitBcsCopyAndD2HMaskGreaterThanAvailableEnginesWhenEnqueueReadThenEnqueueBlitSplitAcrossAvailableEngines) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    debugManager.flags.SplitBcsMaskD2H.set(0b101010);
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;

    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));

    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());
    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    auto cmdQHw = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    const auto gpgpuTaskCountBefore = cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount();
    const auto bcsTaskCountBefore = cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount();

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::localMemory;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(csr1->peekTaskCount(), 1u);
    EXPECT_EQ(csr2->peekTaskCount(), 1u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    EXPECT_EQ(cmdQHw->kernelParams.size.x, 8 * MemoryConstants::megaByte);

    const_cast<StackVec<TagNodeBase *, 32u> &>(cmdQHw->timestampPacketContainer->peekNodes()).clear();
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenEnqueueWriteThenEnqueueBlitSplit) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;

    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));

    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());
    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    auto cmdQHw = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    const auto gpgpuTaskCountBefore = cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount();
    const auto bcsTaskCountBefore = cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount();

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::localMemory;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueWriteBuffer(buffer.get(), CL_FALSE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(csr1->peekTaskCount(), 1u);
    EXPECT_EQ(csr2->peekTaskCount(), 1u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    EXPECT_EQ(cmdQHw->kernelParams.size.x, 8 * MemoryConstants::megaByte);

    const_cast<StackVec<TagNodeBase *, 32u> &>(cmdQHw->timestampPacketContainer->peekNodes()).clear();
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenEnqueueWriteH2HThenEnqueueBlitSplit) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;

    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));

    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());
    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    auto cmdQHw = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    const auto gpgpuTaskCountBefore = cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount();
    const auto bcsTaskCountBefore = cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount();

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::system64KBPages;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueWriteBuffer(buffer.get(), CL_FALSE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(csr1->peekTaskCount(), 1u);
    EXPECT_EQ(csr2->peekTaskCount(), 1u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    EXPECT_EQ(cmdQHw->kernelParams.size.x, 8 * MemoryConstants::megaByte);

    const_cast<StackVec<TagNodeBase *, 32u> &>(cmdQHw->timestampPacketContainer->peekNodes()).clear();
}

HWTEST_F(OoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenEnqueueReadWithRequestedStallingCommandThenEnqueueMarkerBeforeBlitSplit) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.SplitBcsMaskD2H.set(0b1010);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;
    auto cmdQHw = static_cast<MockCommandQueueHw<FamilyType> *>(this->pCmdQ);
    cmdQHw->setStallingCommandsOnNextFlush(true);
    cmdQHw->splitBarrierRequired = true;
    cmdQHw->bcsEngineCount = bcsInfoMaskSize;

    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());

    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    const auto gpgpuTaskCountBefore = cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount();
    const auto bcsTaskCountBefore = cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount();

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::localMemory;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueReadBuffer(buffer.get(), CL_TRUE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(csr1->peekTaskCount(), 2u);
    EXPECT_EQ(csr2->peekTaskCount(), 2u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore + 4u);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);
    EXPECT_FALSE(cmdQHw->splitBarrierRequired);

    pCmdQ->release();
    pCmdQ = nullptr;
}

HWTEST_F(OoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenEnqueueBarrierNonSplitCopyAndSplitCopyThenSplitWaitCorrectly) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    debugManager.flags.SplitBcsMaskD2H.set(0b1010);
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;
    auto cmdQHw = static_cast<MockCommandQueueHw<FamilyType> *>(this->pCmdQ);
    cmdQHw->bcsEngineCount = bcsInfoMaskSize;
    cmdQHw->setStallingCommandsOnNextFlush(true);
    cmdQHw->splitBarrierRequired = true;

    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());

    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    const auto gpgpuTaskCountBefore = cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount();
    const auto bcsTaskCountBefore = cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount();

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::localMemory;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueReadBuffer(buffer.get(), CL_TRUE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore + 2u);
    EXPECT_TRUE(cmdQHw->splitBarrierRequired);

    debugManager.flags.SplitBcsCopy.set(1);

    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueReadBuffer(buffer.get(), CL_TRUE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(csr1->peekTaskCount(), 2u);
    EXPECT_EQ(csr2->peekTaskCount(), 2u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore + 4u);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore + 2u);
    EXPECT_FALSE(cmdQHw->splitBarrierRequired);

    pCmdQ->release();
    pCmdQ = nullptr;
}

HWTEST_F(OoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenEnqueueReadThenDoNotEnqueueMarkerBeforeBlitSplit) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.SplitBcsMaskD2H.set(0b1010);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;
    auto cmdQHw = static_cast<MockCommandQueueHw<FamilyType> *>(this->pCmdQ);
    cmdQHw->bcsEngineCount = bcsInfoMaskSize;

    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());

    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    const auto gpgpuTaskCountBefore = cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount();
    const auto bcsTaskCountBefore = cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount();

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::localMemory;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueReadBuffer(buffer.get(), CL_TRUE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(csr1->peekTaskCount(), 2u);
    EXPECT_EQ(csr2->peekTaskCount(), 2u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    pCmdQ->release();
    pCmdQ = nullptr;
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenEnqueueReadWithRequestedStallingCommandThenDoNotEnqueueMarkerBeforeBlitSplit) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.SplitBcsMaskD2H.set(0b1010);
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;
    auto cmdQHw = static_cast<MockCommandQueueHw<FamilyType> *>(this->pCmdQ);
    cmdQHw->setStallingCommandsOnNextFlush(true);
    cmdQHw->splitBarrierRequired = true;
    cmdQHw->bcsEngineCount = bcsInfoMaskSize;

    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());

    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    const auto gpgpuTaskCountBefore = cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount();
    const auto bcsTaskCountBefore = cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount();

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::localMemory;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueReadBuffer(buffer.get(), CL_TRUE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(csr1->peekTaskCount(), 2u);
    EXPECT_EQ(csr2->peekTaskCount(), 2u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore + 2u);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);
    EXPECT_FALSE(cmdQHw->splitBarrierRequired);

    pCmdQ->release();
    pCmdQ = nullptr;
}

HWTEST_F(OoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenEnqueueReadWithNoRequestedStallingCommandButSplitBarrierRequiredThenEnqueueMarkerBeforeBlitSplit) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.SplitBcsMaskD2H.set(0b1010);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;
    auto cmdQHw = static_cast<MockCommandQueueHw<FamilyType> *>(this->pCmdQ);
    cmdQHw->setStallingCommandsOnNextFlush(false);
    cmdQHw->splitBarrierRequired = true;
    cmdQHw->bcsEngineCount = bcsInfoMaskSize;

    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());

    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    const auto gpgpuTaskCountBefore = cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount();
    const auto bcsTaskCountBefore = cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount();

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::localMemory;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueReadBuffer(buffer.get(), CL_TRUE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(csr1->peekTaskCount(), 2u);
    EXPECT_EQ(csr2->peekTaskCount(), 2u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore + 4u);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);
    EXPECT_FALSE(cmdQHw->splitBarrierRequired);

    pCmdQ->release();
    pCmdQ = nullptr;
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenEnqueueBlockingReadThenEnqueueBlitSplit) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.SplitBcsMaskD2H.set(0b1010);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;
    auto cmdQHw = static_cast<MockCommandQueueHw<FamilyType> *>(this->pCmdQ);
    cmdQHw->bcsEngineCount = bcsInfoMaskSize;

    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());

    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    const auto gpgpuTaskCountBefore = cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount();
    const auto bcsTaskCountBefore = cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount();

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::localMemory;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueReadBuffer(buffer.get(), CL_TRUE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(csr1->peekTaskCount(), 2u);
    EXPECT_EQ(csr2->peekTaskCount(), 2u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    pCmdQ->release();
    pCmdQ = nullptr;
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenEnqueueReadWithEventThenEnqueueBlitSplitAndAddBothTimestampsToEvent) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsCopy.set(1);
    debugManager.flags.SplitBcsMaskD2H.set(0b1010);
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;
    auto cmdQHw = static_cast<MockCommandQueueHw<FamilyType> *>(this->pCmdQ);

    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());

    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    const auto gpgpuTaskCountBefore = cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount();
    const auto bcsTaskCountBefore = cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount();

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::localMemory;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    cl_event event;
    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, &event));

    EXPECT_EQ(csr1->peekTaskCount(), 1u);
    EXPECT_EQ(csr2->peekTaskCount(), 1u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), gpgpuTaskCountBefore);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), bcsTaskCountBefore);

    EXPECT_NE(event, nullptr);
    auto pEvent = castToObject<Event>(event);
    EXPECT_EQ(pEvent->getTimestampPacketNodes()->peekNodes().size(), 3u);
    clReleaseEvent(event);

    pCmdQ->release();
    pCmdQ = nullptr;
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenGpgpuCsrWhenEnqueueingSubsequentBlitsThenGpgpuCommandStreamIsNotObtained) {
    if (pDevice->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    auto &gpgpuCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto srcBuffer = std::unique_ptr<Buffer>{BufferHelper<>::create(pContext)};
    auto dstBuffer = std::unique_ptr<Buffer>{BufferHelper<>::create(pContext)};

    cl_int retVal = pCmdQ->enqueueCopyBuffer(
        srcBuffer.get(),
        dstBuffer.get(),
        0,
        0,
        1,
        0,
        nullptr,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, gpgpuCsr.ensureCommandBufferAllocationCalled);

    retVal = pCmdQ->enqueueCopyBuffer(
        srcBuffer.get(),
        dstBuffer.get(),
        0,
        0,
        1,
        0,
        nullptr,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, gpgpuCsr.ensureCommandBufferAllocationCalled);
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenGpgpuCsrWhenEnqueueingBlitAfterNotFlushedKernelThenGpgpuCommandStreamIsObtained) {
    auto &gpgpuCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto srcBuffer = std::unique_ptr<Buffer>{BufferHelper<>::create(pContext)};
    auto dstBuffer = std::unique_ptr<Buffer>{BufferHelper<>::create(pContext)};
    pCmdQ->getGpgpuCommandStreamReceiver().overrideDispatchPolicy(DispatchMode::batchedDispatch);

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    size_t offset = 0;
    size_t size = 1;
    cl_int retVal = pCmdQ->enqueueKernel(mockKernelWithInternals.mockKernel, 1, &offset, &size, &size, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(0, gpgpuCsr.ensureCommandBufferAllocationCalled);
    const auto ensureCommandBufferAllocationCalledAfterKernel = gpgpuCsr.ensureCommandBufferAllocationCalled;

    retVal = pCmdQ->enqueueCopyBuffer(
        srcBuffer.get(),
        dstBuffer.get(),
        0,
        0,
        1,
        0,
        nullptr,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(ensureCommandBufferAllocationCalledAfterKernel, gpgpuCsr.ensureCommandBufferAllocationCalled);
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenGpgpuCsrWhenEnqueueingBlitAfterFlushedKernelThenGpgpuCommandStreamIsNotObtained) {
    auto &gpgpuCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto srcBuffer = std::unique_ptr<Buffer>{BufferHelper<>::create(pContext)};
    auto dstBuffer = std::unique_ptr<Buffer>{BufferHelper<>::create(pContext)};

    DebugManagerStateRestore restorer;
    debugManager.flags.ForceCacheFlushForBcs.set(0);
    debugManager.flags.EnableBlitterForEnqueueOperations.set(1);

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    size_t offset = 0;
    size_t size = 1;
    cl_int retVal = pCmdQ->enqueueKernel(mockKernelWithInternals.mockKernel, 1, &offset, &size, &size, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(0, gpgpuCsr.ensureCommandBufferAllocationCalled);
    const auto ensureCommandBufferAllocationCalledAfterKernel = gpgpuCsr.ensureCommandBufferAllocationCalled;

    retVal = pCmdQ->enqueueCopyBuffer(
        srcBuffer.get(),
        dstBuffer.get(),
        0,
        0,
        1,
        0,
        nullptr,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(ensureCommandBufferAllocationCalledAfterKernel, gpgpuCsr.ensureCommandBufferAllocationCalled);
}

HWTEST_F(OoqCommandQueueHwBlitTest, givenBlitAfterBarrierWhenEnqueueingCommandThenWaitForBarrierOnBlit) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    if (pCmdQ->getTimestampPacketContainer() == nullptr) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restore{};
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    debugManager.flags.ForceCacheFlushForBcs.set(0);
    debugManager.flags.UpdateTaskCountFromWait.set(1);

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    MockKernel *kernel = mockKernelWithInternals.mockKernel;
    size_t offset = 0;
    size_t gws = 1;
    BufferDefaults::context = context;
    auto buffer = clUniquePtr(BufferHelper<>::create());
    char ptr[1] = {};

    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueKernel(kernel, 1, &offset, &gws, nullptr, 0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueKernel(kernel, 1, &offset, &gws, nullptr, 0, nullptr, nullptr));
    auto ccsStart = pCmdQ->getGpgpuCommandStreamReceiver().getCS().getUsed();
    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueBarrierWithWaitList(0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 1u, ptr, nullptr, 0, nullptr, nullptr));

    uint64_t barrierNodeAddress = 0u;
    {
        HardwareParse ccsHwParser;
        ccsHwParser.parseCommands<FamilyType>(pCmdQ->getGpgpuCommandStreamReceiver().getCS(0), ccsStart);

        const auto pipeControlItor = find<PIPE_CONTROL *>(ccsHwParser.cmdList.begin(), ccsHwParser.cmdList.end());
        auto pipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControlItor);
        EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControl->getPostSyncOperation());
        barrierNodeAddress = pipeControl->getAddress() | (static_cast<uint64_t>(pipeControl->getAddressHigh()) << 32);

        // There shouldn't be any semaphores before the barrier
        const auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(ccsHwParser.cmdList.begin(), pipeControlItor);
        EXPECT_EQ(pipeControlItor, semaphoreItor);
    }

    {
        HardwareParse bcsHwParser;
        bcsHwParser.parseCommands<FamilyType>(pCmdQ->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS)->getCS(0), 0u);

        const auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(bcsHwParser.cmdList.begin(), bcsHwParser.cmdList.end());
        auto semaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
        EXPECT_EQ(barrierNodeAddress, semaphore->getSemaphoreGraphicsAddress());

        const auto pipeControlItor = find<PIPE_CONTROL *>(semaphoreItor, bcsHwParser.cmdList.end());
        EXPECT_EQ(bcsHwParser.cmdList.end(), pipeControlItor);
    }

    EXPECT_EQ(CL_SUCCESS, pCmdQ->finish(false));
}

HWTEST_F(OoqCommandQueueHwBlitTest, givenBlitBeforeBarrierWhenEnqueueingCommandThenWaitForBlitBeforeBarrier) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    if (pCmdQ->getTimestampPacketContainer() == nullptr) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restore{};
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    debugManager.flags.ForceCacheFlushForBcs.set(0);
    debugManager.flags.UpdateTaskCountFromWait.set(1);

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    MockKernel *kernel = mockKernelWithInternals.mockKernel;
    size_t offset = 0;
    size_t gws = 1;
    BufferDefaults::context = context;
    auto buffer = clUniquePtr(BufferHelper<>::create());
    char ptr[1] = {};

    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 1u, ptr, nullptr, 0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 1u, ptr, nullptr, 0, nullptr, nullptr));
    uint64_t lastBlitNodeAddress = TimestampPacketHelper::getContextEndGpuAddress(*pCmdQ->getTimestampPacketContainer()->peekNodes()[0]);
    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueKernel(kernel, 1, &offset, &gws, nullptr, 0, nullptr, nullptr));

    auto bcsStart = pCmdQ->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS)->getCS(0).getUsed();
    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueBarrierWithWaitList(0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueKernel(kernel, 1, &offset, &gws, nullptr, 0, nullptr, nullptr));

    uint64_t barrierNodeAddress = 0u;
    {
        HardwareParse queueHwParser;
        queueHwParser.parseCommands<FamilyType>(*pDevice->getUltCommandStreamReceiver<FamilyType>().lastFlushedCommandStream, 0);

        const auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(queueHwParser.cmdList.begin(), queueHwParser.cmdList.end());
        const auto semaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
        EXPECT_EQ(lastBlitNodeAddress, semaphore->getSemaphoreGraphicsAddress());

        const auto pipeControlItor = find<PIPE_CONTROL *>(semaphoreItor, queueHwParser.cmdList.end());
        const auto pipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControlItor);
        EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControl->getPostSyncOperation());
        barrierNodeAddress = pipeControl->getAddress() | (static_cast<uint64_t>(pipeControl->getAddressHigh()) << 32);

        // There shouldn't be any more semaphores before the barrier
        EXPECT_EQ(pipeControlItor, find<MI_SEMAPHORE_WAIT *>(std::next(semaphoreItor), pipeControlItor));

        // Make sure the gpgpu semaphore is programmed before the second compute walker
        auto itor = queueHwParser.cmdList.begin();
        auto semaphoreIndex = 0u;
        auto lastComputeWalkerIndex = 0u;
        auto index = 0u;
        while (itor != queueHwParser.cmdList.end()) {
            const auto semaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
            if (semaphore) {
                semaphoreIndex = index;
            }
            if (NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(itor, queueHwParser.cmdList.end()) != queueHwParser.cmdList.end()) {
                lastComputeWalkerIndex = index;
            }

            ++itor;
            ++index;
        }
        EXPECT_LT(semaphoreIndex, lastComputeWalkerIndex);
    }

    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 1u, ptr, nullptr, 0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 1u, ptr, nullptr, 0, nullptr, nullptr));

    {
        HardwareParse bcsHwParser;
        bcsHwParser.parseCommands<FamilyType>(pCmdQ->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS)->getCS(0), bcsStart);

        const auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(bcsHwParser.cmdList.begin(), bcsHwParser.cmdList.end());
        const auto semaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
        EXPECT_EQ(barrierNodeAddress, semaphore->getSemaphoreGraphicsAddress());
        EXPECT_EQ(bcsHwParser.cmdList.end(), find<PIPE_CONTROL *>(semaphoreItor, bcsHwParser.cmdList.end()));

        // Only one barrier semaphore from first BCS enqueue
        const auto blitItor = find<XY_COPY_BLT *>(bcsHwParser.cmdList.begin(), bcsHwParser.cmdList.end());
        EXPECT_EQ(1u, findAll<MI_SEMAPHORE_WAIT *>(bcsHwParser.cmdList.begin(), blitItor).size());
    }

    EXPECT_EQ(CL_SUCCESS, pCmdQ->finish(false));
}

HWTEST_F(OoqCommandQueueHwBlitTest, givenBlockedBlitAfterBarrierWhenEnqueueingCommandThenWaitForBlitBeforeBarrier) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    if (pCmdQ->getTimestampPacketContainer() == nullptr) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restore{};
    debugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    debugManager.flags.ForceCacheFlushForBcs.set(0);
    debugManager.flags.UpdateTaskCountFromWait.set(1);

    UserEvent userEvent;
    cl_event userEventWaitlist[] = {&userEvent};
    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    MockKernel *kernel = mockKernelWithInternals.mockKernel;
    size_t offset = 0;
    size_t gws = 1;
    BufferDefaults::context = context;
    auto buffer = clUniquePtr(BufferHelper<>::create());
    char ptr[1] = {};

    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 1u, ptr, nullptr, 0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 1u, ptr, nullptr, 0, nullptr, nullptr));
    uint64_t lastBlitNodeAddress = TimestampPacketHelper::getContextEndGpuAddress(*pCmdQ->getTimestampPacketContainer()->peekNodes()[0]);
    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueKernel(kernel, 1, &offset, &gws, nullptr, 0, nullptr, nullptr));
    auto ccsStart = pCmdQ->getGpgpuCommandStreamReceiver().getCS().getUsed();
    auto bcsStart = pCmdQ->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS)->getCS(0).getUsed();
    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueBarrierWithWaitList(0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 1u, ptr, nullptr, 1, userEventWaitlist, nullptr));

    userEvent.setStatus(CL_COMPLETE);

    uint64_t barrierNodeAddress = 0u;
    {
        HardwareParse ccsHwParser;
        ccsHwParser.parseCommands<FamilyType>(pCmdQ->getGpgpuCommandStreamReceiver().getCS(0), ccsStart);

        const auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(ccsHwParser.cmdList.begin(), ccsHwParser.cmdList.end());
        const auto semaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
        EXPECT_EQ(lastBlitNodeAddress, semaphore->getSemaphoreGraphicsAddress());

        const auto pipeControlItor = find<PIPE_CONTROL *>(semaphoreItor, ccsHwParser.cmdList.end());
        const auto pipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControlItor);
        EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControl->getPostSyncOperation());
        barrierNodeAddress = pipeControl->getAddress() | (static_cast<uint64_t>(pipeControl->getAddressHigh()) << 32);

        // There shouldn't be any more semaphores before the barrier
        EXPECT_EQ(pipeControlItor, find<MI_SEMAPHORE_WAIT *>(std::next(semaphoreItor), pipeControlItor));
    }

    {
        HardwareParse bcsHwParser;
        bcsHwParser.parseCommands<FamilyType>(pCmdQ->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS)->getCS(0), bcsStart);

        const auto semaphoreItor = find<MI_SEMAPHORE_WAIT *>(bcsHwParser.cmdList.begin(), bcsHwParser.cmdList.end());
        const auto semaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphoreItor);
        EXPECT_EQ(barrierNodeAddress, semaphore->getSemaphoreGraphicsAddress());
        EXPECT_EQ(bcsHwParser.cmdList.end(), find<PIPE_CONTROL *>(semaphoreItor, bcsHwParser.cmdList.end()));
    }

    EXPECT_EQ(CL_SUCCESS, pCmdQ->finish(false));
}

HWTEST_F(CommandQueueHwTest, GivenBuiltinKernelWhenBuiltinDispatchInfoBuilderIsProvidedThenThisBuilderIsUsedForCreatingDispatchInfo) {
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);

    MockKernelWithInternals mockKernelToUse(*pClDevice);
    MockBuilder builder(*pDevice->getBuiltIns(), *pClDevice);
    builder.paramsToUse.gws.x = 11;
    builder.paramsToUse.elws.x = 13;
    builder.paramsToUse.offset.x = 17;
    builder.paramsToUse.kernel = mockKernelToUse.mockKernel;

    MockKernelWithInternals mockKernelToSend(*pClDevice);
    mockKernelToSend.kernelInfo.builtinDispatchBuilder = &builder;
    NullSurface s;
    Surface *surfaces[] = {&s};
    size_t gws[3] = {3, 0, 0};
    size_t lws[3] = {5, 0, 0};
    size_t off[3] = {7, 0, 0};

    EXPECT_FALSE(builder.wasBuildDispatchInfosWithBuiltinOpParamsCalled);
    EXPECT_FALSE(builder.wasBuildDispatchInfosWithKernelParamsCalled);

    cmdQHw->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(surfaces, false, mockKernelToSend.mockKernel, 1, off, gws, lws, lws, 0, nullptr, nullptr);

    EXPECT_FALSE(builder.wasBuildDispatchInfosWithBuiltinOpParamsCalled);
    EXPECT_TRUE(builder.wasBuildDispatchInfosWithKernelParamsCalled);

    EXPECT_EQ(Vec3<size_t>(gws[0], gws[1], gws[2]), builder.paramsReceived.gws);
    EXPECT_EQ(Vec3<size_t>(lws[0], lws[1], lws[2]), builder.paramsReceived.elws);
    EXPECT_EQ(Vec3<size_t>(off[0], off[1], off[2]), builder.paramsReceived.offset);
    EXPECT_EQ(mockKernelToSend.mockKernel, builder.paramsReceived.kernel);

    auto dispatchInfo = builder.paramsReceived.multiDispatchInfo.begin();
    EXPECT_EQ(1U, builder.paramsReceived.multiDispatchInfo.size());
    EXPECT_EQ(builder.paramsToUse.gws.x, dispatchInfo->getGWS().x);
    EXPECT_EQ(builder.paramsToUse.elws.x, dispatchInfo->getEnqueuedWorkgroupSize().x);
    EXPECT_EQ(builder.paramsToUse.offset.x, dispatchInfo->getOffset().x);
    EXPECT_EQ(builder.paramsToUse.kernel, dispatchInfo->getKernel());
}

struct ImageTextureCacheFlushTest : public CommandQueueHwBlitTest<false> {
    void SetUp() override {
        REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
        MockExecutionEnvironment mockExecutionEnvironment{};
        auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHelper<ProductHelper>();
        if (!productHelper.isBlitterForImagesSupported() || !productHelper.blitEnqueuePreferred(false)) {
            GTEST_SKIP();
        }

        CommandQueueHwBlitTest<false>::SetUp();
        debugManager.flags.ForceCacheFlushForBcs.set(0);
    }

    void TearDown() override {
        if (IsSkipped()) {
            return;
        }

        CommandQueueHwBlitTest<false>::TearDown();
    }

    template <typename FamilyType>
    void submitKernel(bool usingImages) {
        MockKernelWithInternals kernelInternals(*pClDevice, context);
        kernelInternals.mockKernel->usingImages = usingImages;
        Kernel *kernel = kernelInternals.mockKernel;
        MockMultiDispatchInfo multiDispatchInfo(pClDevice, kernel);
        auto mockCmdQ = static_cast<MockCommandQueueHw<FamilyType> *>(this->pCmdQ);
        auto enqueueResult = mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr, 0, false, multiDispatchInfo, 0, nullptr, nullptr);
        EXPECT_EQ(CL_SUCCESS, enqueueResult);
    }

    DebugManagerStateRestore restorer;
};

HWTEST_F(ImageTextureCacheFlushTest, givenTextureCacheFlushNotRequiredWhenEnqueueWriteImageThenNoCacheFlushSubmitted) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    if (pCmdQ->getTimestampPacketContainer() == nullptr) {
        GTEST_SKIP();
    }

    std::unique_ptr<Image> dstImage(ImageHelperUlt<ImageUseHostPtr<Image2dDefaults>>::create(context));
    auto imageDesc = dstImage->getImageDesc();
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, 0};
    char ptr[1] = {};

    submitKernel<FamilyType>(false);

    auto cmdQStart = pCmdQ->getCS(0).getUsed();
    auto status = pCmdQ->enqueueWriteImage(dstImage.get(),
                                           CL_FALSE,
                                           origin,
                                           region,
                                           0,
                                           0,
                                           ptr,
                                           nullptr,
                                           0,
                                           0,
                                           nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    LinearStream &cmdQStream = pCmdQ->getCS(0);
    HardwareParse ccsHwParser;
    ccsHwParser.parseCommands<FamilyType>(cmdQStream, cmdQStart);

    auto pipeControls = findAll<PIPE_CONTROL *>(ccsHwParser.cmdList.begin(), ccsHwParser.cmdList.end());
    EXPECT_TRUE(pipeControls.empty());
    EXPECT_EQ(CL_SUCCESS, pCmdQ->finish(false));
}

HWTEST_F(ImageTextureCacheFlushTest, givenTextureCacheFlushRequiredWhenEnqueueReadImageThenNoCacheFlushSubmitted) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    if (pCmdQ->getTimestampPacketContainer() == nullptr) {
        GTEST_SKIP();
    }

    std::unique_ptr<Image> srcImage(ImageHelperUlt<ImageUseHostPtr<Image2dDefaults>>::create(context));
    auto imageDesc = srcImage->getImageDesc();
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, 0};
    char ptr[1] = {};

    submitKernel<FamilyType>(true);

    auto cmdQStart = pCmdQ->getCS(0).getUsed();
    auto status = pCmdQ->enqueueReadImage(srcImage.get(),
                                          CL_FALSE,
                                          origin,
                                          region,
                                          0,
                                          0,
                                          ptr,
                                          nullptr,
                                          0,
                                          0,
                                          nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    LinearStream &cmdQStream = pCmdQ->getCS(0);
    HardwareParse ccsHwParser;
    ccsHwParser.parseCommands<FamilyType>(cmdQStream, cmdQStart);

    auto pipeControls = findAll<PIPE_CONTROL *>(ccsHwParser.cmdList.begin(), ccsHwParser.cmdList.end());
    EXPECT_TRUE(pipeControls.empty());
    EXPECT_EQ(CL_SUCCESS, pCmdQ->finish(false));
}

HWTEST_F(ImageTextureCacheFlushTest, givenTextureCacheFlushRequiredWhenEnqueueWriteImageThenCacheFlushSubmitted) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    if (pCmdQ->getTimestampPacketContainer() == nullptr) {
        GTEST_SKIP();
    }

    std::unique_ptr<Image> dstImage(ImageHelperUlt<ImageUseHostPtr<Image2dDefaults>>::create(context));
    auto imageDesc = dstImage->getImageDesc();
    size_t origin[] = {0, 0, 0};
    size_t region[] = {imageDesc.image_width, imageDesc.image_height, 0};
    char ptr[1] = {};

    submitKernel<FamilyType>(true);

    auto cmdQStart = pCmdQ->getCS(0).getUsed();
    auto status = pCmdQ->enqueueWriteImage(dstImage.get(),
                                           CL_FALSE,
                                           origin,
                                           region,
                                           0,
                                           0,
                                           ptr,
                                           nullptr,
                                           0,
                                           0,
                                           nullptr);
    EXPECT_EQ(CL_SUCCESS, status);

    LinearStream &cmdQStream = pCmdQ->getCS(0);
    HardwareParse ccsHwParser;
    ccsHwParser.parseCommands<FamilyType>(cmdQStream, cmdQStart);

    bool isPipeControlWithTextureCacheFlush = false;
    auto pipeControls = findAll<PIPE_CONTROL *>(ccsHwParser.cmdList.begin(), ccsHwParser.cmdList.end());
    EXPECT_FALSE(pipeControls.empty());
    for (auto pipeControlIter : pipeControls) {
        auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*pipeControlIter);
        if (0u == pipeControlCmd->getImmediateData() &&
            PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA == pipeControlCmd->getPostSyncOperation() &&
            pipeControlCmd->getTextureCacheInvalidationEnable()) {
            isPipeControlWithTextureCacheFlush = true;
            break;
        }
    }
    EXPECT_TRUE(isPipeControlWithTextureCacheFlush);
    EXPECT_EQ(CL_SUCCESS, pCmdQ->finish(false));
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenImageWithHostPtrWhenCreateImageThenStopRegularBcs) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
    auto &engine = pDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular);
    auto mockCsr = reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(engine.commandStreamReceiver);
    mockCsr->blitterDirectSubmissionAvailable = true;
    mockCsr->blitterDirectSubmission = std::make_unique<MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>>>(*mockCsr);
    auto directSubmission = reinterpret_cast<MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> *>(mockCsr->blitterDirectSubmission.get());
    directSubmission->initialize(true);

    EXPECT_TRUE(directSubmission->ringStart);
    std::unique_ptr<Image> image(ImageHelperUlt<ImageUseHostPtr<Image2dDefaults>>::create(context));
    EXPECT_FALSE(directSubmission->ringStart);
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenBufferWithHostPtrWhenCreateBufferThenStopRegularBcs) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::cpuAccessDisallowed));

    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->localMemorySupported[pDevice->getRootDeviceIndex()] = true;

    auto &engine = pDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular);
    auto mockCsr = reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(engine.commandStreamReceiver);
    mockCsr->blitterDirectSubmissionAvailable = true;
    mockCsr->blitterDirectSubmission = std::make_unique<MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>>>(*mockCsr);
    auto directSubmission = reinterpret_cast<MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>> *>(mockCsr->blitterDirectSubmission.get());
    directSubmission->initialize(true);

    EXPECT_TRUE(directSubmission->ringStart);
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(context));
    EXPECT_FALSE(directSubmission->ringStart);
}
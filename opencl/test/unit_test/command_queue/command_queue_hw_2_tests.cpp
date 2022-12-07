/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/helpers/dispatch_info_builder.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

void cloneMdi(MultiDispatchInfo &dst, const MultiDispatchInfo &src) {
    for (auto &srcDi : src) {
        dst.push(srcDi);
    }
    dst.setBuiltinOpParams(src.peekBuiltinOpParams());
}

struct MockBuilder : BuiltinDispatchInfoBuilder {
    using BuiltinDispatchInfoBuilder::BuiltinDispatchInfoBuilder;
    bool buildDispatchInfos(MultiDispatchInfo &d) const override {
        wasBuildDispatchInfosWithBuiltinOpParamsCalled = true;
        paramsReceived.multiDispatchInfo.setBuiltinOpParams(d.peekBuiltinOpParams());
        return true;
    }
    bool buildDispatchInfos(MultiDispatchInfo &d, Kernel *kernel,
                            const uint32_t dim, const Vec3<size_t> &gws, const Vec3<size_t> &elws, const Vec3<size_t> &offset) const override {
        paramsReceived.kernel = kernel;
        paramsReceived.gws = gws;
        paramsReceived.elws = elws;
        paramsReceived.offset = offset;
        wasBuildDispatchInfosWithKernelParamsCalled = true;

        DispatchInfoBuilder<NEO::SplitDispatch::Dim::d3D, NEO::SplitDispatch::SplitMode::NoSplit> dispatchInfoBuilder(clDevice);
        dispatchInfoBuilder.setKernel(paramsToUse.kernel);
        dispatchInfoBuilder.setDispatchGeometry(dim, paramsToUse.gws, paramsToUse.elws, paramsToUse.offset);
        dispatchInfoBuilder.bake(d);

        cloneMdi(paramsReceived.multiDispatchInfo, d);
        return true;
    }

    mutable bool wasBuildDispatchInfosWithBuiltinOpParamsCalled = false;
    mutable bool wasBuildDispatchInfosWithKernelParamsCalled = false;
    struct Params {
        MultiDispatchInfo multiDispatchInfo;
        Kernel *kernel = nullptr;
        Vec3<size_t> gws = Vec3<size_t>{0, 0, 0};
        Vec3<size_t> elws = Vec3<size_t>{0, 0, 0};
        Vec3<size_t> offset = Vec3<size_t>{0, 0, 0};
    };

    mutable Params paramsReceived;
    Params paramsToUse;
};

struct BuiltinParamsCommandQueueHwTests : public CommandQueueHwTest {

    void setUpImpl(EBuiltInOps::Type operation) {
        auto builtIns = new MockBuiltins();
        pCmdQ->getDevice().getExecutionEnvironment()->rootDeviceEnvironments[pCmdQ->getDevice().getRootDeviceIndex()]->builtins.reset(builtIns);

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

    setUpImpl(EBuiltInOps::CopyBufferToBuffer);
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

    setUpImpl(EBuiltInOps::CopyBufferToImage3d);

    std::unique_ptr<Image> dstImage(ImageHelper<ImageUseHostPtr<Image2dDefaults>>::create(context));

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

HWTEST_F(BuiltinParamsCommandQueueHwTests, givenEnqueueReadImageCallWhenBuiltinParamsArePassedThenCheckValuesCorectness) {

    setUpImpl(EBuiltInOps::CopyImage3dToBuffer);

    std::unique_ptr<Image> dstImage(ImageHelper<ImageUseHostPtr<Image2dDefaults>>::create(context));

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

    setUpImpl(EBuiltInOps::CopyBufferRect);

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

HWTEST_F(OOQueueHwTest, givenBlockedOutOfOrderCmdQueueAndAsynchronouslyCompletedEventWhenEnqueueCompletesVirtualEventThenUpdatedTaskLevelIsPassedToEnqueueAndFlushTask) {
    CommandQueueHw<FamilyType> *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);

    int32_t executionStamp = 0;
    auto mockCSR = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCSR);

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;
    size_t offset = 0;
    size_t size = 1;

    class MockEventWithSetCompleteOnUpdate : public Event {
      public:
        MockEventWithSetCompleteOnUpdate(CommandQueue *cmdQueue, cl_command_type cmdType,
                                         uint32_t taskLevel, uint32_t taskCount) : Event(cmdQueue, cmdType, taskLevel, taskCount) {
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
    //new virtual event is created on enqueue, bind it to the created virtual event
    EXPECT_NE(cmdQHw->virtualEvent, &virtualEvent);

    event.setStatus(CL_SUBMITTED);

    virtualEvent.Event::updateExecutionStatus();
    EXPECT_FALSE(cmdQHw->isQueueBlocked());

    //+1 due to dependency between virtual event & new virtual event
    //new virtual event is actually responsible for command delivery
    EXPECT_EQ(virtualEventTaskLevel + 1, cmdQHw->taskLevel);
    EXPECT_EQ(virtualEventTaskLevel + 1, mockCSR->lastTaskLevelToFlushTask);
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenCheckIsSplitEnqueueBlitSupportedThenReturnProperValue) {
    DebugManagerStateRestore restorer;
    auto *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);
    {
        DebugManager.flags.SplitBcsCopy.set(1);
        EXPECT_TRUE(cmdQHw->getDevice().isBcsSplitSupported());
    }
    {
        DebugManager.flags.SplitBcsCopy.set(0);
        EXPECT_FALSE(cmdQHw->getDevice().isBcsSplitSupported());
    }
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenCheckIsSplitEnqueueBlitNeededThenReturnProperValue) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);
    auto *cmdQHw = static_cast<CommandQueueHw<FamilyType> *>(this->pCmdQ);
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    ultHwConfig.useBlitSplit = true;
    {
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::HostToHost, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::LocalToLocal, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_TRUE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::LocalToHost, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_TRUE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::HostToLocal, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
    }
    {
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::HostToHost, 64 * MemoryConstants::megaByte, cmdQHw->getGpgpuCommandStreamReceiver()));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::LocalToLocal, 64 * MemoryConstants::megaByte, cmdQHw->getGpgpuCommandStreamReceiver()));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::LocalToHost, 64 * MemoryConstants::megaByte, cmdQHw->getGpgpuCommandStreamReceiver()));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::HostToLocal, 64 * MemoryConstants::megaByte, cmdQHw->getGpgpuCommandStreamReceiver()));
    }
    {
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::HostToHost, 4 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::LocalToLocal, 4 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::LocalToHost, 4 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::HostToLocal, 4 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
    }
    {
        DebugManager.flags.SplitBcsCopy.set(0);
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::HostToHost, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::LocalToLocal, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::LocalToHost, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::HostToLocal, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
    }
    {
        DebugManager.flags.SplitBcsCopy.set(-1);
        ultHwConfig.useBlitSplit = false;
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::HostToHost, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::LocalToLocal, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::LocalToHost, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
        EXPECT_FALSE(cmdQHw->isSplitEnqueueBlitNeeded(TransferDirection::HostToLocal, 64 * MemoryConstants::megaByte, *cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)));
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
    DebugManager.flags.SplitBcsCopy.set(1);
    DebugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    DebugManager.flags.UpdateTaskCountFromWait.set(3);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;
    auto cmdQHw = std::make_unique<MockCommandQueueHw<FamilyType>>(context, pClDevice, nullptr);

    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());

    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::LocalMemory;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), 0u);

    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(csr1->peekTaskCount(), 1u);
    EXPECT_EQ(csr2->peekTaskCount(), 1u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), 0u);

    EXPECT_EQ(cmdQHw->kernelParams.size.x, 8 * MemoryConstants::megaByte);

    const_cast<StackVec<TagNodeBase *, 32u> &>(cmdQHw->timestampPacketContainer->peekNodes()).clear();
}

HWTEST_F(OoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenEnqueueReadWithRequestedStallingCommandThenEnqueueMarkerBeforeBlitSplit) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);
    DebugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    DebugManager.flags.UpdateTaskCountFromWait.set(3);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;
    auto cmdQHw = static_cast<MockCommandQueueHw<FamilyType> *>(this->pCmdQ);
    cmdQHw->getGpgpuCommandStreamReceiver().requestStallingCommandsOnNextFlush();

    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());

    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::LocalMemory;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), 0u);

    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueReadBuffer(buffer.get(), CL_TRUE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(csr1->peekTaskCount(), 2u);
    EXPECT_EQ(csr2->peekTaskCount(), 2u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), 4u);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), 0u);

    pCmdQ->release();
    pCmdQ = nullptr;
}

HWTEST_F(OoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenEnqueueReadThenDoNotEnqueueMarkerBeforeBlitSplit) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);
    DebugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    DebugManager.flags.UpdateTaskCountFromWait.set(3);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;
    auto cmdQHw = static_cast<MockCommandQueueHw<FamilyType> *>(this->pCmdQ);

    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());

    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::LocalMemory;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), 0u);

    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueReadBuffer(buffer.get(), CL_TRUE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(csr1->peekTaskCount(), 2u);
    EXPECT_EQ(csr2->peekTaskCount(), 2u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), 0u);

    pCmdQ->release();
    pCmdQ = nullptr;
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenEnqueueReadWithRequestedStallingCommandThenDoNotEnqueueMarkerBeforeBlitSplit) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);
    DebugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    DebugManager.flags.UpdateTaskCountFromWait.set(3);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;
    auto cmdQHw = static_cast<MockCommandQueueHw<FamilyType> *>(this->pCmdQ);
    cmdQHw->getGpgpuCommandStreamReceiver().requestStallingCommandsOnNextFlush();

    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());

    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::LocalMemory;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), 0u);

    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueReadBuffer(buffer.get(), CL_TRUE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(csr1->peekTaskCount(), 2u);
    EXPECT_EQ(csr2->peekTaskCount(), 2u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), 2u);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), 0u);

    pCmdQ->release();
    pCmdQ = nullptr;
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenEnqueueBlockingReadThenEnqueueBlitSplit) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);
    DebugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    DebugManager.flags.UpdateTaskCountFromWait.set(3);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;
    auto cmdQHw = static_cast<MockCommandQueueHw<FamilyType> *>(this->pCmdQ);

    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());

    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::LocalMemory;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), 0u);

    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueReadBuffer(buffer.get(), CL_TRUE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(csr1->peekTaskCount(), 2u);
    EXPECT_EQ(csr2->peekTaskCount(), 2u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), 0u);

    pCmdQ->release();
    pCmdQ = nullptr;
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenSplitBcsCopyWhenEnqueueReadWithEventThenEnqueueBlitSplitAndAddBothTimestampsToEvent) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SplitBcsCopy.set(1);
    DebugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    DebugManager.flags.UpdateTaskCountFromWait.set(3);
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;
    auto cmdQHw = static_cast<MockCommandQueueHw<FamilyType> *>(this->pCmdQ);

    auto csr1 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext1(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS1, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    csr1->setupContext(*osContext1);
    csr1->initializeTagAllocation();
    EngineControl control1(csr1.get(), osContext1.get());

    auto csr2 = std::make_unique<CommandStreamReceiverHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::unique_ptr<OsContext> osContext2(OsContext::create(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.get(), pDevice->getRootDeviceIndex(), 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS3, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, pDevice->getDeviceBitfield())));
    csr2->setupContext(*osContext2);
    csr2->initializeTagAllocation();
    EngineControl control2(csr2.get(), osContext2.get());

    cmdQHw->bcsEngines[1] = &control1;
    cmdQHw->bcsEngines[3] = &control2;

    BcsSplitBufferTraits::context = context;
    auto buffer = clUniquePtr(BufferHelper<BcsSplitBufferTraits>::create());
    static_cast<MockGraphicsAllocation *>(buffer->getGraphicsAllocation(0u))->memoryPool = MemoryPool::LocalMemory;
    char ptr[1] = {};

    EXPECT_EQ(csr1->peekTaskCount(), 0u);
    EXPECT_EQ(csr2->peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), 0u);

    cl_event event;
    EXPECT_EQ(CL_SUCCESS, cmdQHw->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 16 * MemoryConstants::megaByte, ptr, nullptr, 0, nullptr, &event));

    EXPECT_EQ(csr1->peekTaskCount(), 1u);
    EXPECT_EQ(csr2->peekTaskCount(), 1u);
    EXPECT_EQ(cmdQHw->getGpgpuCommandStreamReceiver().peekTaskCount(), 0u);
    EXPECT_EQ(cmdQHw->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->peekTaskCount(), 0u);

    EXPECT_NE(event, nullptr);
    auto pEvent = castToObject<Event>(event);
    EXPECT_EQ(pEvent->getTimestampPacketNodes()->peekNodes().size(), 3u);
    clReleaseEvent(event);

    pCmdQ->release();
    pCmdQ = nullptr;
}

HWTEST_F(IoqCommandQueueHwBlitTest, givenGpgpuCsrWhenEnqueueingSubsequentBlitsThenGpgpuCommandStreamIsNotObtained) {
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
    pCmdQ->getGpgpuCommandStreamReceiver().overrideDispatchPolicy(DispatchMode::BatchedDispatch);

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
    DebugManager.flags.ForceCacheFlushForBcs.set(0);

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
    DebugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    DebugManager.flags.ForceCacheFlushForBcs.set(0);
    DebugManager.flags.UpdateTaskCountFromWait.set(1);

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

    EXPECT_EQ(CL_SUCCESS, pCmdQ->finish());
}

HWTEST_F(OoqCommandQueueHwBlitTest, givenBlitBeforeBarrierWhenEnqueueingCommandThenWaitForBlitBeforeBarrier) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    if (pCmdQ->getTimestampPacketContainer() == nullptr) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restore{};
    DebugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    DebugManager.flags.ForceCacheFlushForBcs.set(0);
    DebugManager.flags.UpdateTaskCountFromWait.set(1);

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
    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueKernel(kernel, 1, &offset, &gws, nullptr, 0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 1u, ptr, nullptr, 0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 1u, ptr, nullptr, 0, nullptr, nullptr));

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

        // Only one barrier semaphore from first BCS enqueue
        const auto blitItor = find<XY_COPY_BLT *>(bcsHwParser.cmdList.begin(), bcsHwParser.cmdList.end());
        EXPECT_EQ(1u, findAll<MI_SEMAPHORE_WAIT *>(bcsHwParser.cmdList.begin(), blitItor).size());
    }

    EXPECT_EQ(CL_SUCCESS, pCmdQ->finish());
}

HWTEST_F(OoqCommandQueueHwBlitTest, givenBlockedBlitAfterBarrierWhenEnqueueingCommandThenWaitForBlitBeforeBarrier) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    if (pCmdQ->getTimestampPacketContainer() == nullptr) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restore{};
    DebugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    DebugManager.flags.ForceCacheFlushForBcs.set(0);
    DebugManager.flags.UpdateTaskCountFromWait.set(1);

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

    EXPECT_EQ(CL_SUCCESS, pCmdQ->finish());
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

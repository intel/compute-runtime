/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/aub_subcapture.h"
#include "runtime/event/user_event.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/fixtures/enqueue_handler_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_aub_csr.h"
#include "unit_tests/mocks/mock_aub_subcapture_manager.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"

using namespace NEO;

HWTEST_F(EnqueueHandlerTest, enqueueHandlerWithKernelCallsProcessEvictionOnCSR) {
    int32_t tag;
    auto csr = new MockCsrBase<FamilyType>(tag, *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(csr);

    MockKernelWithInternals mockKernel(*pDevice);
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));

    size_t gws[] = {1, 1, 1};
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_TRUE(csr->processEvictionCalled);
}

HWTEST_F(EnqueueHandlerTest, givenEnqueueHandlerWithKernelWhenAubCsrIsActiveThenAddCommentWithKernelName) {
    int32_t tag;
    auto aubCsr = new MockCsrAub<FamilyType>(tag, *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(aubCsr);

    MockKernelWithInternals mockKernel(*pDevice);
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));

    size_t gws[] = {1, 1, 1};
    mockKernel.kernelInfo.name = "kernel_name";
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_TRUE(aubCsr->addAubCommentCalled);

    EXPECT_EQ(1u, aubCsr->aubCommentMessages.size());
    EXPECT_STREQ("kernel_name", aubCsr->aubCommentMessages[0].c_str());
}

HWTEST_F(EnqueueHandlerTest, givenEnqueueHandlerWithKernelSplitWhenAubCsrIsActiveThenAddCommentWithKernelName) {
    int32_t tag;
    auto aubCsr = new MockCsrAub<FamilyType>(tag, *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(aubCsr);

    MockKernelWithInternals kernel1(*pDevice);
    MockKernelWithInternals kernel2(*pDevice);
    kernel1.kernelInfo.name = "kernel_1";
    kernel2.kernelInfo.name = "kernel_2";

    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));
    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({kernel1.mockKernel, kernel2.mockKernel}));

    mockCmdQ->template enqueueHandler<CL_COMMAND_WRITE_BUFFER>(nullptr, 0, true, multiDispatchInfo, 0, nullptr, nullptr);

    EXPECT_TRUE(aubCsr->addAubCommentCalled);

    EXPECT_EQ(2u, aubCsr->aubCommentMessages.size());
    EXPECT_STREQ("kernel_1", aubCsr->aubCommentMessages[0].c_str());
    EXPECT_STREQ("kernel_2", aubCsr->aubCommentMessages[1].c_str());
}

HWTEST_F(EnqueueHandlerTest, givenEnqueueHandlerWithEmptyDispatchInfoWhenAubCsrIsActiveThenDontAddCommentWithKernelName) {
    int32_t tag;
    auto aubCsr = new MockCsrAub<FamilyType>(tag, *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(aubCsr);

    MockKernelWithInternals mockKernel(*pDevice);
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));

    size_t gws[] = {0, 0, 0};
    mockKernel.kernelInfo.name = "kernel_name";
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_FALSE(aubCsr->addAubCommentCalled);
}

struct EnqueueHandlerWithAubSubCaptureTests : public EnqueueHandlerTest {
    template <typename FamilyType>
    class MockCmdQWithAubSubCapture : public CommandQueueHw<FamilyType> {
      public:
        MockCmdQWithAubSubCapture(Context *context, Device *device) : CommandQueueHw<FamilyType>(context, device, nullptr) {}

        void waitUntilComplete(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep) override {
            waitUntilCompleteCalled = true;
            CommandQueueHw<FamilyType>::waitUntilComplete(taskCountToWait, flushStampToWait, useQuickKmdSleep);
        }

        void obtainNewTimestampPacketNodes(size_t numberOfNodes, TimestampPacketContainer &previousNodes, bool clearAllDependencies) override {
            timestampPacketDependenciesCleared = clearAllDependencies;
            CommandQueueHw<FamilyType>::obtainNewTimestampPacketNodes(numberOfNodes, previousNodes, clearAllDependencies);
        }

        bool waitUntilCompleteCalled = false;
        bool timestampPacketDependenciesCleared = false;
    };
};

HWTEST_F(EnqueueHandlerWithAubSubCaptureTests, givenEnqueueHandlerWithAubSubCaptureWhenSubCaptureIsNotActiveThenEnqueueIsMadeBlocking) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(1);

    auto aubCsr = new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(aubCsr);

    AubSubCaptureCommon subCaptureCommon;
    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelName = "invalid_kernel_name";
    auto subCaptureManagerMock = new AubSubCaptureManagerMock("file_name.aub", subCaptureCommon);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

    MockCmdQWithAubSubCapture<FamilyType> cmdQ(context, pDevice);
    MockKernelWithInternals mockKernel(*pDevice);
    size_t gws[3] = {1, 0, 0};
    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_TRUE(cmdQ.waitUntilCompleteCalled);
}

HWTEST_F(EnqueueHandlerWithAubSubCaptureTests, givenEnqueueHandlerWithAubSubCaptureWhenSubCaptureGetsActivatedThenTimestampPacketDependenciesAreClearedAndNextRemainUncleared) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(1);
    DebugManager.flags.EnableTimestampPacket.set(true);

    auto aubCsr = new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(aubCsr);

    AubSubCaptureCommon subCaptureCommon;
    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelName = "";
    subCaptureCommon.subCaptureFilter.dumpKernelStartIdx = 0;
    subCaptureCommon.subCaptureFilter.dumpKernelEndIdx = 1;
    auto subCaptureManagerMock = new AubSubCaptureManagerMock("file_name.aub", subCaptureCommon);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

    MockCmdQWithAubSubCapture<FamilyType> cmdQ(context, pDevice);
    MockKernelWithInternals mockKernel(*pDevice);
    size_t gws[3] = {1, 0, 0};

    // activate subcapture
    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_TRUE(cmdQ.timestampPacketDependenciesCleared);

    // keep subcapture active
    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_FALSE(cmdQ.timestampPacketDependenciesCleared);

    // deactivate subcapture
    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_FALSE(cmdQ.timestampPacketDependenciesCleared);
}

template <typename GfxFamily>
class MyCommandQueueHw : public CommandQueueHw<GfxFamily> {
    typedef CommandQueueHw<GfxFamily> BaseClass;

  public:
    MyCommandQueueHw(Context *context, Device *device, cl_queue_properties *properties) : BaseClass(context, device, properties){};
    Vec3<size_t> lws = {1, 1, 1};
    Vec3<size_t> elws = {1, 1, 1};
    void enqueueHandlerHook(const unsigned int commandType, const MultiDispatchInfo &multiDispatchInfo) override {
        elws = multiDispatchInfo.begin()->getEnqueuedWorkgroupSize();
        lws = multiDispatchInfo.begin()->getActualWorkgroupSize();
    }
};

HWTEST_F(EnqueueHandlerTest, givenLocalWorkgroupSizeGreaterThenGlobalWorkgroupSizeWhenEnqueueKernelThenLwsIsClamped) {
    int32_t tag;
    auto csr = new MockCsrBase<FamilyType>(tag, *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(csr);
    MockKernelWithInternals mockKernel(*pDevice);
    auto mockProgram = mockKernel.mockProgram;
    mockProgram->setAllowNonUniform(true);
    MyCommandQueueHw<FamilyType> myCmdQ(context, pDevice, 0);

    size_t lws1d[] = {4, 1, 1};
    size_t gws1d[] = {2, 1, 1};
    myCmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws1d, lws1d, 0, nullptr, nullptr);
    EXPECT_EQ(myCmdQ.elws.x, lws1d[0]);
    EXPECT_EQ(myCmdQ.lws.x, gws1d[0]);

    size_t lws2d[] = {3, 3, 1};
    size_t gws2d[] = {2, 1, 1};
    myCmdQ.enqueueKernel(mockKernel.mockKernel, 2, nullptr, gws2d, lws2d, 0, nullptr, nullptr);
    EXPECT_EQ(myCmdQ.elws.x, lws2d[0]);
    EXPECT_EQ(myCmdQ.elws.y, lws2d[1]);
    EXPECT_EQ(myCmdQ.lws.x, gws2d[0]);
    EXPECT_EQ(myCmdQ.lws.y, gws2d[1]);

    size_t lws3d[] = {5, 4, 3};
    size_t gws3d[] = {2, 2, 2};
    myCmdQ.enqueueKernel(mockKernel.mockKernel, 3, nullptr, gws3d, lws3d, 0, nullptr, nullptr);
    EXPECT_EQ(myCmdQ.elws.x, lws3d[0]);
    EXPECT_EQ(myCmdQ.elws.y, lws3d[1]);
    EXPECT_EQ(myCmdQ.elws.z, lws3d[2]);
    EXPECT_EQ(myCmdQ.lws.x, gws3d[0]);
    EXPECT_EQ(myCmdQ.lws.y, gws3d[1]);
    EXPECT_EQ(myCmdQ.lws.z, gws3d[2]);
}

HWTEST_F(EnqueueHandlerTest, givenLocalWorkgroupSizeGreaterThenGlobalWorkgroupSizeAndNonUniformWorkGroupWhenEnqueueKernelThenClIvalidWorkGroupSizeIsReturned) {
    int32_t tag;
    auto csr = new MockCsrBase<FamilyType>(tag, *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(csr);
    MockKernelWithInternals mockKernel(*pDevice);
    auto mockProgram = mockKernel.mockProgram;
    mockProgram->setAllowNonUniform(false);
    MyCommandQueueHw<FamilyType> myCmdQ(context, pDevice, 0);

    size_t lws1d[] = {4, 1, 1};
    size_t gws1d[] = {2, 1, 1};
    cl_int retVal = myCmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws1d, lws1d, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_INVALID_WORK_GROUP_SIZE);
}

HWTEST_F(EnqueueHandlerTest, enqueueHandlerCallOnEnqueueMarkerDoesntCallProcessEvictionOnCSR) {
    int32_t tag;
    auto csr = new MockCsrBase<FamilyType>(tag, *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(csr);

    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));

    mockCmdQ->enqueueMarkerWithWaitList(
        0,
        nullptr,
        nullptr);

    EXPECT_FALSE(csr->processEvictionCalled);
    EXPECT_EQ(0u, csr->madeResidentGfxAllocations.size());
    EXPECT_EQ(0u, csr->madeNonResidentGfxAllocations.size());
}

HWTEST_F(EnqueueHandlerTest, enqueueHandlerForMarkerOnUnblockedQueueDoesntIncrementTaskLevel) {
    int32_t tag;
    auto csr = new MockCsrBase<FamilyType>(tag, *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(csr);

    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));

    // put queue into initial unblocked state
    mockCmdQ->taskLevel = 0;

    mockCmdQ->enqueueMarkerWithWaitList(
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(0u, mockCmdQ->taskLevel);
}

HWTEST_F(EnqueueHandlerTest, enqueueHandlerForMarkerOnBlockedQueueShouldNotIncrementTaskLevel) {
    int32_t tag;
    auto csr = new MockCsrBase<FamilyType>(tag, *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(csr);

    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));

    // put queue into initial blocked state
    mockCmdQ->taskLevel = Event::eventNotReady;

    mockCmdQ->enqueueMarkerWithWaitList(
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(Event::eventNotReady, mockCmdQ->taskLevel);
}

HWTEST_F(EnqueueHandlerTest, enqueueBlockedWithoutReturnEventCreatesVirtualEventAndIncremetsCommandQueueInternalRefCount) {

    MockKernelWithInternals kernelInternals(*pDevice, context);

    Kernel *kernel = kernelInternals.mockKernel;

    MockMultiDispatchInfo multiDispatchInfo(kernel);

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pDevice, 0);

    // put queue into initial blocked state
    mockCmdQ->taskLevel = Event::eventNotReady;

    auto initialRefCountInternal = mockCmdQ->getRefInternalCount();

    bool blocking = false;
    mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr,
                                                                 0,
                                                                 blocking,
                                                                 multiDispatchInfo,
                                                                 0,
                                                                 nullptr,
                                                                 nullptr);

    EXPECT_NE(nullptr, mockCmdQ->virtualEvent);

    auto refCountInternal = mockCmdQ->getRefInternalCount();
    EXPECT_EQ(initialRefCountInternal + 1, refCountInternal);

    mockCmdQ->virtualEvent->setStatus(CL_COMPLETE);
    mockCmdQ->isQueueBlocked();
    mockCmdQ->release();
}

HWTEST_F(EnqueueHandlerTest, enqueueBlockedSetsVirtualEventAsCurrentCmdQVirtualEvent) {

    MockKernelWithInternals kernelInternals(*pDevice, context);

    Kernel *kernel = kernelInternals.mockKernel;

    MockMultiDispatchInfo multiDispatchInfo(kernel);

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pDevice, 0);

    // put queue into initial blocked state
    mockCmdQ->taskLevel = Event::eventNotReady;

    bool blocking = false;
    mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr,
                                                                 0,
                                                                 blocking,
                                                                 multiDispatchInfo,
                                                                 0,
                                                                 nullptr,
                                                                 nullptr);

    ASSERT_NE(nullptr, mockCmdQ->virtualEvent);

    mockCmdQ->virtualEvent->setStatus(CL_COMPLETE);
    mockCmdQ->isQueueBlocked();
    mockCmdQ->release();
}

HWTEST_F(EnqueueHandlerTest, enqueueWithOutputEventRegistersEvent) {
    MockKernelWithInternals kernelInternals(*pDevice, context);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);
    cl_event outputEvent = nullptr;

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pDevice, 0);

    bool blocking = false;
    mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr,
                                                                 0,
                                                                 blocking,
                                                                 multiDispatchInfo,
                                                                 0,
                                                                 nullptr,
                                                                 &outputEvent);
    ASSERT_NE(nullptr, outputEvent);
    Event *event = castToObjectOrAbort<Event>(outputEvent);
    ASSERT_NE(nullptr, event);

    event->release();
    mockCmdQ->release();
}

HWTEST_F(EnqueueHandlerTest, givenEnqueueHandlerWhenAddPatchInfoCommentsForAUBDumpIsNotSetThenPatchInfoDataIsNotTransferredToCSR) {
    auto csr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment);
    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment);
    csr->overwriteFlatBatchBufferHelper(mockHelper);
    pDevice->resetCommandStreamReceiver(csr);

    MockKernelWithInternals mockKernel(*pDevice);
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));

    size_t gws[] = {1, 1, 1};

    PatchInfoData patchInfoData = {0xaaaaaaaa, 0, PatchInfoAllocationType::KernelArg, 0xbbbbbbbb, 0, PatchInfoAllocationType::IndirectObjectHeap};
    mockKernel.mockKernel->getPatchInfoDataList().push_back(patchInfoData);

    EXPECT_CALL(*mockHelper, setPatchInfoData(::testing::_)).Times(0);
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
}

HWTEST_F(EnqueueHandlerTest, givenEnqueueHandlerWhenAddPatchInfoCommentsForAUBDumpIsSetThenPatchInfoDataIsTransferredToCSR) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);

    auto csr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment);
    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment);
    csr->overwriteFlatBatchBufferHelper(mockHelper);
    pDevice->resetCommandStreamReceiver(csr);

    MockKernelWithInternals mockKernel(*pDevice);
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));

    size_t gws[] = {1, 1, 1};

    PatchInfoData patchInfoData = {0xaaaaaaaa, 0, PatchInfoAllocationType::KernelArg, 0xbbbbbbbb, 0, PatchInfoAllocationType::IndirectObjectHeap};
    mockKernel.mockKernel->getPatchInfoDataList().push_back(patchInfoData);

    EXPECT_CALL(*mockHelper, setPatchInfoData(::testing::_)).Times(8);
    EXPECT_CALL(*mockHelper, registerCommandChunk(::testing::_)).Times(1);
    EXPECT_CALL(*mockHelper, registerBatchBufferStartAddress(::testing::_, ::testing::_)).Times(1);
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
}

HWTEST_F(EnqueueHandlerTest, givenExternallySynchronizedParentEventWhenRequestingEnqueueWithoutGpuSubmissionThenTaskCountIsNotInherited) {
    struct ExternallySynchEvent : VirtualEvent {
        ExternallySynchEvent(CommandQueue *cmdQueue) {
            setStatus(CL_COMPLETE);
            this->updateTaskCount(7);
        }
        bool isExternallySynchronized() const override {
            return true;
        }
    };

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pDevice, 0);

    ExternallySynchEvent synchEvent(mockCmdQ);
    cl_event inEv = &synchEvent;
    cl_event outEv = nullptr;

    bool blocking = false;
    MultiDispatchInfo emptyDispatchInfo;
    mockCmdQ->template enqueueHandler<CL_COMMAND_MARKER>(nullptr,
                                                         0,
                                                         blocking,
                                                         emptyDispatchInfo,
                                                         1U,
                                                         &inEv,
                                                         &outEv);
    Event *ouputEvent = castToObject<Event>(outEv);
    ASSERT_NE(nullptr, ouputEvent);
    EXPECT_EQ(0U, ouputEvent->peekTaskCount());

    ouputEvent->release();
    mockCmdQ->release();
}

HWTEST_F(EnqueueHandlerTest, givenEnqueueHandlerWhenSubCaptureIsOffThenActivateSubCaptureIsNotCalled) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(static_cast<int32_t>(AubSubCaptureManager::SubCaptureMode::Off));

    MockKernelWithInternals kernelInternals(*pDevice, context);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pDevice, 0);

    mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr,
                                                                 0,
                                                                 false,
                                                                 multiDispatchInfo,
                                                                 0,
                                                                 nullptr,
                                                                 nullptr);
    EXPECT_FALSE(pDevice->getUltCommandStreamReceiver<FamilyType>().checkAndActivateAubSubCaptureCalled);

    mockCmdQ->release();
}

HWTEST_F(EnqueueHandlerTest, givenEnqueueHandlerWhenSubCaptureIsOnThenActivateSubCaptureIsCalled) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(static_cast<int32_t>(AubSubCaptureManager::SubCaptureMode::Filter));

    MockKernelWithInternals kernelInternals(*pDevice, context);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pDevice, 0);

    mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr,
                                                                 0,
                                                                 false,
                                                                 multiDispatchInfo,
                                                                 0,
                                                                 nullptr,
                                                                 nullptr);
    EXPECT_TRUE(pDevice->getUltCommandStreamReceiver<FamilyType>().checkAndActivateAubSubCaptureCalled);

    mockCmdQ->release();
}
using EnqueueHandlerTestBasic = ::testing::Test;
HWTEST_F(EnqueueHandlerTestBasic, givenEnqueueHandlerWhenCommandIsBlokingThenCompletionStampTaskCountIsPassedToWaitForTaskCountAndCleanAllocationListAsRequiredTaskCount) {
    int32_t tag;
    auto executionEnvironment = platformImpl->peekExecutionEnvironment();
    auto mockCsr = new MockCsrBase<FamilyType>(tag, *executionEnvironment);
    executionEnvironment->commandStreamReceivers.resize(1);
    std::unique_ptr<MockDevice> pDevice(MockDevice::createWithExecutionEnvironment<MockDevice>(nullptr, executionEnvironment, 0u));
    pDevice->resetCommandStreamReceiver(mockCsr);
    auto context = std::make_unique<MockContext>(pDevice.get());
    MockKernelWithInternals kernelInternals(*pDevice, context.get());
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context.get(), pDevice.get(), 0);
    mockCmdQ->deltaTaskCount = 100;
    mockCmdQ->template enqueueHandler<CL_COMMAND_WRITE_BUFFER>(nullptr,
                                                               0,
                                                               true,
                                                               multiDispatchInfo,
                                                               0,
                                                               nullptr,
                                                               nullptr);
    EXPECT_EQ(mockCsr->waitForTaskCountRequiredTaskCount, mockCmdQ->completionStampTaskCount);
    mockCmdQ->release();
}

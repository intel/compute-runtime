/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_subcapture.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/program/sync_buffer_handler.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_aub_subcapture_manager.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_internal_allocation_storage.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/event/user_event.h"
#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/command_stream/thread_arbitration_policy_helper.h"
#include "opencl/test/unit_test/fixtures/enqueue_handler_fixture.h"
#include "opencl/test/unit_test/helpers/cl_hw_parse.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "test_traits_common.h"

using namespace NEO;

HWTEST_F(EnqueueHandlerTest, WhenEnqueingHandlerWithKernelThenProcessEvictionOnCsrIsCalled) {
    int32_t tag;
    auto csr = new MockCsrBase<FamilyType>(tag, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(csr);

    MockKernelWithInternals mockKernel(*pClDevice);
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pClDevice, 0));

    size_t gws[] = {1, 1, 1};
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_TRUE(csr->processEvictionCalled);
}

HWTEST_F(EnqueueHandlerTest, givenEnqueueHandlerWithKernelWhenAubCsrIsActiveThenAddCommentWithKernelName) {
    int32_t tag;
    auto aubCsr = new MockCsrAub<FamilyType>(tag, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(aubCsr);

    MockKernelWithInternals mockKernel(*pClDevice);
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pClDevice, 0));

    size_t gws[] = {1, 1, 1};
    mockKernel.kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "kernel_name";
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_TRUE(aubCsr->addAubCommentCalled);

    EXPECT_EQ(1u, aubCsr->aubCommentMessages.size());
    EXPECT_STREQ("kernel_name", aubCsr->aubCommentMessages[0].c_str());
}

HWTEST_F(EnqueueHandlerTest, givenEnqueueHandlerWithKernelSplitWhenAubCsrIsActiveThenAddCommentWithKernelName) {
    int32_t tag;
    auto aubCsr = new MockCsrAub<FamilyType>(tag, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(aubCsr);

    MockKernelWithInternals kernel1(*pClDevice);
    MockKernelWithInternals kernel2(*pClDevice);
    kernel1.kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "kernel_1";
    kernel2.kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "kernel_2";

    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pClDevice, 0));
    MockMultiDispatchInfo multiDispatchInfo(pClDevice, std::vector<Kernel *>({kernel1.mockKernel, kernel2.mockKernel}));

    const auto enqueueResult = mockCmdQ->template enqueueHandler<CL_COMMAND_WRITE_BUFFER>(nullptr, 0, true, multiDispatchInfo, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, enqueueResult);
    EXPECT_TRUE(aubCsr->addAubCommentCalled);

    EXPECT_EQ(2u, aubCsr->aubCommentMessages.size());
    EXPECT_STREQ("kernel_1", aubCsr->aubCommentMessages[0].c_str());
    EXPECT_STREQ("kernel_2", aubCsr->aubCommentMessages[1].c_str());
}

HWTEST_F(EnqueueHandlerTest, givenEnqueueHandlerWithEmptyDispatchInfoWhenAubCsrIsActiveThenDontAddCommentWithKernelName) {
    int32_t tag;
    auto aubCsr = new MockCsrAub<FamilyType>(tag, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(aubCsr);

    MockKernelWithInternals mockKernel(*pClDevice);
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pClDevice, 0));

    size_t gws[] = {0, 0, 0};
    mockKernel.kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "kernel_name";
    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_FALSE(aubCsr->addAubCommentCalled);
}

struct EnqueueHandlerWithAubSubCaptureTests : public EnqueueHandlerTest {
    template <typename FamilyType>
    class MockCmdQWithAubSubCapture : public CommandQueueHw<FamilyType> {
      public:
        MockCmdQWithAubSubCapture(Context *context, ClDevice *device) : CommandQueueHw<FamilyType>(context, device, nullptr, false) {}

        WaitStatus waitUntilComplete(uint32_t gpgpuTaskCountToWait, Range<CopyEngineState> copyEnginesToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool cleanTemporaryAllocationList, bool skipWait) override {
            waitUntilCompleteCalled = true;
            return CommandQueueHw<FamilyType>::waitUntilComplete(gpgpuTaskCountToWait, copyEnginesToWait, flushStampToWait, useQuickKmdSleep, cleanTemporaryAllocationList, skipWait);
        }

        void obtainNewTimestampPacketNodes(size_t numberOfNodes, TimestampPacketContainer &previousNodes, bool clearAllDependencies, CommandStreamReceiver &csr) override {
            timestampPacketDependenciesCleared = clearAllDependencies;
            CommandQueueHw<FamilyType>::obtainNewTimestampPacketNodes(numberOfNodes, previousNodes, clearAllDependencies, csr);
        }

        bool waitUntilCompleteCalled = false;
        bool timestampPacketDependenciesCleared = false;
    };
};

HWTEST_F(EnqueueHandlerWithAubSubCaptureTests, givenEnqueueHandlerWithAubSubCaptureWhenSubCaptureIsNotActiveThenEnqueueIsMadeBlocking) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(1);

    auto aubCsr = new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(aubCsr);

    AubSubCaptureCommon subCaptureCommon;
    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelName = "invalid_kernel_name";
    auto subCaptureManagerMock = new AubSubCaptureManagerMock("file_name.aub", subCaptureCommon);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

    MockCmdQWithAubSubCapture<FamilyType> cmdQ(context, pClDevice);
    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};
    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_TRUE(cmdQ.waitUntilCompleteCalled);
}

HWTEST_F(EnqueueHandlerWithAubSubCaptureTests, givenEnqueueMarkerWithAubSubCaptureWhenSubCaptureIsNotActiveThenEnqueueIsMadeBlocking) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(1);

    auto aubCsr = new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(aubCsr);

    AubSubCaptureCommon subCaptureCommon;
    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelName = "invalid_kernel_name";
    auto subCaptureManagerMock = new AubSubCaptureManagerMock("file_name.aub", subCaptureCommon);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

    MockCmdQWithAubSubCapture<FamilyType> cmdQ(context, pClDevice);
    cmdQ.enqueueMarkerWithWaitList(0, nullptr, nullptr);

    EXPECT_TRUE(cmdQ.waitUntilCompleteCalled);
}

HWTEST_F(EnqueueHandlerWithAubSubCaptureTests, givenEnqueueHandlerWithAubSubCaptureWhenSubCaptureGetsActivatedThenTimestampPacketDependenciesAreClearedAndNextRemainUncleared) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(1);
    DebugManager.flags.EnableTimestampPacket.set(true);

    auto aubCsr = new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(aubCsr);

    AubSubCaptureCommon subCaptureCommon;
    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelName = "";
    subCaptureCommon.subCaptureFilter.dumpKernelStartIdx = 0;
    subCaptureCommon.subCaptureFilter.dumpKernelEndIdx = 1;
    auto subCaptureManagerMock = new AubSubCaptureManagerMock("file_name.aub", subCaptureCommon);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

    MockCmdQWithAubSubCapture<FamilyType> cmdQ(context, pClDevice);
    MockKernelWithInternals mockKernel(*pClDevice);
    mockKernel.kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "kernelName";
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

HWTEST_F(EnqueueHandlerWithAubSubCaptureTests, givenInputEventsWhenDispatchingEnqueueWithSubCaptureThenClearDependencies) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(1);
    DebugManager.flags.EnableTimestampPacket.set(true);

    auto defaultEngine = defaultHwInfo->capabilityTable.defaultEngineType;

    MockOsContext mockOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({defaultEngine, EngineUsage::Regular}));

    auto aubCsr = new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto aubCsr2 = std::make_unique<MockAubCsr<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    aubCsr->setupContext(mockOsContext);
    aubCsr2->setupContext(mockOsContext);

    pDevice->resetCommandStreamReceiver(aubCsr);

    AubSubCaptureCommon subCaptureCommon;
    subCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Filter;
    subCaptureCommon.subCaptureFilter.dumpKernelName = "";
    subCaptureCommon.subCaptureFilter.dumpKernelStartIdx = 0;
    subCaptureCommon.subCaptureFilter.dumpKernelEndIdx = 1;
    auto subCaptureManagerMock = new AubSubCaptureManagerMock("file_name.aub", subCaptureCommon);
    aubCsr->subCaptureManager.reset(subCaptureManagerMock);

    MockCmdQWithAubSubCapture<FamilyType> cmdQ(context, pClDevice);
    MockKernelWithInternals mockKernel(*pClDevice);
    mockKernel.kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "kernelName";
    size_t gws[3] = {1, 0, 0};

    MockTimestampPacketContainer onCsrTimestamp(*aubCsr->getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer outOfCsrTimestamp(*aubCsr2->getTimestampPacketAllocator(), 1);

    Event event1(&cmdQ, 0, 0, 0);
    Event event2(&cmdQ, 0, 0, 0);
    event1.addTimestampPacketNodes(onCsrTimestamp);
    event1.addTimestampPacketNodes(outOfCsrTimestamp);

    cl_event waitlist[] = {&event1, &event2};

    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 2, waitlist, nullptr);
    EXPECT_TRUE(cmdQ.timestampPacketDependenciesCleared);

    CsrDependencies &outOfCsrDeps = aubCsr->recordedDispatchFlags.csrDependencies;
    EXPECT_EQ(0u, outOfCsrDeps.timestampPacketContainer.size());
}

template <typename GfxFamily>
class MyCommandQueueHw : public CommandQueueHw<GfxFamily> {
    typedef CommandQueueHw<GfxFamily> BaseClass;

  public:
    MyCommandQueueHw(Context *context, ClDevice *device, cl_queue_properties *properties) : BaseClass(context, device, properties, false){};
    Vec3<size_t> lws = {1, 1, 1};
    Vec3<size_t> elws = {1, 1, 1};
    void enqueueHandlerHook(const unsigned int commandType, const MultiDispatchInfo &multiDispatchInfo) override {
        elws = multiDispatchInfo.begin()->getEnqueuedWorkgroupSize();
        lws = multiDispatchInfo.begin()->getActualWorkgroupSize();
    }
};

HWTEST_F(EnqueueHandlerTest, givenLocalWorkgroupSizeGreaterThenGlobalWorkgroupSizeWhenEnqueueKernelThenLwsIsClamped) {
    int32_t tag;
    auto csr = new MockCsrBase<FamilyType>(tag, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(csr);
    MockKernelWithInternals mockKernel(*pClDevice);
    auto mockProgram = mockKernel.mockProgram;
    mockProgram->setAllowNonUniform(true);
    MyCommandQueueHw<FamilyType> myCmdQ(context, pClDevice, 0);

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
    auto csr = new MockCsrBase<FamilyType>(tag, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(csr);
    MockKernelWithInternals mockKernel(*pClDevice);
    auto mockProgram = mockKernel.mockProgram;
    mockProgram->setAllowNonUniform(false);
    MyCommandQueueHw<FamilyType> myCmdQ(context, pClDevice, 0);

    size_t lws1d[] = {4, 1, 1};
    size_t gws1d[] = {2, 1, 1};
    cl_int retVal = myCmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws1d, lws1d, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_INVALID_WORK_GROUP_SIZE);
}

HWTEST_F(EnqueueHandlerTest, WhenEnqueuingHandlerCallOnEnqueueMarkerThenCallProcessEvictionOnCsrIsNotCalled) {
    int32_t tag;
    auto csr = new MockCsrBase<FamilyType>(tag, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(csr);

    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pClDevice, 0));

    mockCmdQ->enqueueMarkerWithWaitList(
        0,
        nullptr,
        nullptr);

    EXPECT_FALSE(csr->processEvictionCalled);
    EXPECT_EQ(0u, csr->madeResidentGfxAllocations.size());
    EXPECT_EQ(0u, csr->madeNonResidentGfxAllocations.size());
}

HWTEST_F(EnqueueHandlerTest, WhenEnqueuingHandlerForMarkerOnUnblockedQueueThenTaskLevelIsNotIncremented) {
    int32_t tag;
    auto csr = new MockCsrBase<FamilyType>(tag, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(csr);

    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pClDevice, 0));

    // put queue into initial unblocked state
    mockCmdQ->taskLevel = 0;

    mockCmdQ->enqueueMarkerWithWaitList(
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(0u, mockCmdQ->taskLevel);
}

HWTEST_F(EnqueueHandlerTest, WhenEnqueuingHandlerForMarkerOnBlockedQueueThenTaskLevelIsNotIncremented) {
    int32_t tag;
    auto csr = new MockCsrBase<FamilyType>(tag, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(csr);

    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pClDevice, 0));

    // put queue into initial blocked state
    mockCmdQ->taskLevel = CompletionStamp::notReady;

    mockCmdQ->enqueueMarkerWithWaitList(
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CompletionStamp::notReady, mockCmdQ->taskLevel);
}

HWTEST_F(EnqueueHandlerTest, WhenEnqueuingBlockedWithoutReturnEventThenVirtualEventIsCreatedAndCommandQueueInternalRefCountIsIncremeted) {

    MockKernelWithInternals kernelInternals(*pClDevice, context);

    Kernel *kernel = kernelInternals.mockKernel;

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, kernel);

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pClDevice, 0);

    // put queue into initial blocked state
    mockCmdQ->taskLevel = CompletionStamp::notReady;

    auto initialRefCountInternal = mockCmdQ->getRefInternalCount();

    bool blocking = false;
    const auto enqueueResult = mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr,
                                                                                            0,
                                                                                            blocking,
                                                                                            multiDispatchInfo,
                                                                                            0,
                                                                                            nullptr,
                                                                                            nullptr);
    EXPECT_EQ(CL_SUCCESS, enqueueResult);

    EXPECT_NE(nullptr, mockCmdQ->virtualEvent);

    auto refCountInternal = mockCmdQ->getRefInternalCount();
    EXPECT_EQ(initialRefCountInternal + 1, refCountInternal);

    mockCmdQ->virtualEvent->setStatus(CL_COMPLETE);
    mockCmdQ->isQueueBlocked();
    mockCmdQ->release();
}

HWTEST_F(EnqueueHandlerTest, WhenEnqueuingBlockedThenVirtualEventIsSetAsCurrentCmdQVirtualEvent) {

    MockKernelWithInternals kernelInternals(*pClDevice, context);

    Kernel *kernel = kernelInternals.mockKernel;

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, kernel);

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pClDevice, 0);

    // put queue into initial blocked state
    mockCmdQ->taskLevel = CompletionStamp::notReady;

    bool blocking = false;
    const auto enqueueResult = mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr,
                                                                                            0,
                                                                                            blocking,
                                                                                            multiDispatchInfo,
                                                                                            0,
                                                                                            nullptr,
                                                                                            nullptr);
    EXPECT_EQ(CL_SUCCESS, enqueueResult);

    ASSERT_NE(nullptr, mockCmdQ->virtualEvent);

    mockCmdQ->virtualEvent->setStatus(CL_COMPLETE);
    mockCmdQ->isQueueBlocked();
    mockCmdQ->release();
}

HWTEST_F(EnqueueHandlerTest, WhenEnqueuingWithOutputEventThenEventIsRegistered) {
    MockKernelWithInternals kernelInternals(*pClDevice, context);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(pClDevice, kernel);
    cl_event outputEvent = nullptr;

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pClDevice, 0);

    bool blocking = false;
    const auto enqueueResult = mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr,
                                                                                            0,
                                                                                            blocking,
                                                                                            multiDispatchInfo,
                                                                                            0,
                                                                                            nullptr,
                                                                                            &outputEvent);
    EXPECT_EQ(CL_SUCCESS, enqueueResult);

    ASSERT_NE(nullptr, outputEvent);
    Event *event = castToObjectOrAbort<Event>(outputEvent);
    ASSERT_NE(nullptr, event);

    event->release();
    mockCmdQ->release();
}

HWTEST_F(EnqueueHandlerTest, givenEnqueueHandlerWhenAddPatchInfoCommentsForAUBDumpIsNotSetThenPatchInfoDataIsNotTransferredToCSR) {
    auto csr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment);
    csr->overwriteFlatBatchBufferHelper(mockHelper);
    pDevice->resetCommandStreamReceiver(csr);

    MockKernelWithInternals mockKernel(*pClDevice);
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pClDevice, 0));

    size_t gws[] = {1, 1, 1};

    PatchInfoData patchInfoData = {0xaaaaaaaa, 0, PatchInfoAllocationType::KernelArg, 0xbbbbbbbb, 0, PatchInfoAllocationType::IndirectObjectHeap};
    mockKernel.mockKernel->getPatchInfoDataList().push_back(patchInfoData);

    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, mockHelper->setPatchInfoDataCalled);
}

HWTEST2_F(EnqueueHandlerTest, givenEnqueueHandlerWhenAddPatchInfoCommentsForAUBDumpIsSetThenPatchInfoDataIsTransferredToCSR, MatchAny) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);

    auto csr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*pDevice->executionEnvironment);
    csr->overwriteFlatBatchBufferHelper(mockHelper);
    pDevice->resetCommandStreamReceiver(csr);

    MockKernelWithInternals mockKernel(*pClDevice);
    auto mockCmdQ = std::unique_ptr<MockCommandQueueHw<FamilyType>>(new MockCommandQueueHw<FamilyType>(context, pClDevice, 0));

    size_t gws[] = {1, 1, 1};

    PatchInfoData patchInfoData = {0xaaaaaaaa, 0, PatchInfoAllocationType::KernelArg, 0xbbbbbbbb, 0, PatchInfoAllocationType::IndirectObjectHeap};
    mockKernel.mockKernel->getPatchInfoDataList().push_back(patchInfoData);

    uint32_t expectedCallsCount = TestTraits<gfxCoreFamily>::iohInSbaSupported ? 8 : 7;
    if (!pDevice->getHardwareInfo().capabilityTable.supportsImages) {
        --expectedCallsCount;
    }

    mockCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(expectedCallsCount, mockHelper->setPatchInfoDataCalled);
    EXPECT_EQ(1u, mockHelper->registerCommandChunkCalled);
    EXPECT_EQ(1u, mockHelper->registerBatchBufferStartAddressCalled);
}

HWTEST_F(EnqueueHandlerTest, givenExternallySynchronizedParentEventWhenRequestingEnqueueWithoutGpuSubmissionThenTaskCountIsNotInherited) {
    struct ExternallySynchEvent : UserEvent {
        ExternallySynchEvent() : UserEvent() {
            setStatus(CL_COMPLETE);
            this->updateTaskCount(7, 0);
        }
        bool isExternallySynchronized() const override {
            return true;
        }
    };

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pClDevice, 0);

    ExternallySynchEvent synchEvent;
    cl_event inEv = &synchEvent;
    cl_event outEv = nullptr;

    bool blocking = false;
    MultiDispatchInfo emptyDispatchInfo;
    const auto enqueueResult = mockCmdQ->template enqueueHandler<CL_COMMAND_MARKER>(nullptr,
                                                                                    0,
                                                                                    blocking,
                                                                                    emptyDispatchInfo,
                                                                                    1U,
                                                                                    &inEv,
                                                                                    &outEv);
    EXPECT_EQ(CL_SUCCESS, enqueueResult);

    Event *ouputEvent = castToObject<Event>(outEv);
    ASSERT_NE(nullptr, ouputEvent);
    EXPECT_EQ(0U, ouputEvent->peekTaskCount());

    ouputEvent->release();
    mockCmdQ->release();
}

HWTEST_F(EnqueueHandlerTest, givenEnqueueHandlerWhenSubCaptureIsOffThenActivateSubCaptureIsNotCalled) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(static_cast<int32_t>(AubSubCaptureManager::SubCaptureMode::Off));

    MockKernelWithInternals kernelInternals(*pClDevice, context);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(pClDevice, kernel);

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pClDevice, 0);

    const auto enqueueResult = mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr,
                                                                                            0,
                                                                                            false,
                                                                                            multiDispatchInfo,
                                                                                            0,
                                                                                            nullptr,
                                                                                            nullptr);
    EXPECT_EQ(CL_SUCCESS, enqueueResult);
    EXPECT_FALSE(pDevice->getUltCommandStreamReceiver<FamilyType>().checkAndActivateAubSubCaptureCalled);

    mockCmdQ->release();
}

HWTEST_F(EnqueueHandlerTest, givenEnqueueHandlerWhenSubCaptureIsOnThenActivateSubCaptureIsCalled) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(static_cast<int32_t>(AubSubCaptureManager::SubCaptureMode::Filter));

    MockKernelWithInternals kernelInternals(*pClDevice, context);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(pClDevice, kernel);

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pClDevice, 0);

    const auto enqueueResult = mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr,
                                                                                            0,
                                                                                            false,
                                                                                            multiDispatchInfo,
                                                                                            0,
                                                                                            nullptr,
                                                                                            nullptr);
    EXPECT_EQ(CL_SUCCESS, enqueueResult);
    EXPECT_TRUE(pDevice->getUltCommandStreamReceiver<FamilyType>().checkAndActivateAubSubCaptureCalled);

    mockCmdQ->release();
}

HWTEST_F(EnqueueHandlerTest, givenEnqueueHandlerWhenClSetKernelExecInfoAlreadySetKernelThreadArbitrationPolicyThenRequiredThreadArbitrationPolicyIsSetProperly) {
    REQUIRE_SVM_OR_SKIP(pClDevice);
    auto &hwHelper = NEO::ClHwHelper::get(pClDevice->getHardwareInfo().platform.eRenderCoreFamily);
    if (!hwHelper.isSupportedKernelThreadArbitrationPolicy()) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(static_cast<int32_t>(AubSubCaptureManager::SubCaptureMode::Filter));
    DebugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(1);

    MockKernelWithInternals kernelInternals(*pClDevice, context);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(pClDevice, kernel);

    uint32_t euThreadSetting = CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL;
    size_t ptrSizeInBytes = 1 * sizeof(uint32_t *);
    clSetKernelExecInfo(
        kernelInternals.mockMultiDeviceKernel,               // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_INTEL, // cl_kernel_exec_info param_name
        ptrSizeInBytes,                                      // size_t param_value_size
        &euThreadSetting                                     // const void *param_value
    );
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pClDevice, 0);

    const auto enqueueResult = mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr,
                                                                                            0,
                                                                                            false,
                                                                                            multiDispatchInfo,
                                                                                            0,
                                                                                            nullptr,
                                                                                            nullptr);
    EXPECT_EQ(CL_SUCCESS, enqueueResult);
    EXPECT_EQ(getNewKernelArbitrationPolicy(euThreadSetting),
              pDevice->getUltCommandStreamReceiver<FamilyType>().streamProperties.stateComputeMode.threadArbitrationPolicy.value);

    mockCmdQ->release();
}

HWTEST_F(EnqueueHandlerTest, givenEnqueueHandlerWhenNotSupportedPolicyChangeThenRequiredThreadArbitrationPolicyNotChangedAndIsSetAsDefault) {
    auto &hwHelper = NEO::ClHwHelper::get(pClDevice->getHardwareInfo().platform.eRenderCoreFamily);
    if (hwHelper.isSupportedKernelThreadArbitrationPolicy()) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(static_cast<int32_t>(AubSubCaptureManager::SubCaptureMode::Filter));

    MockKernelWithInternals kernelInternals(*pClDevice, context);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(pClDevice, kernel);

    uint32_t euThreadSetting = CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL;
    size_t ptrSizeInBytes = 1 * sizeof(uint32_t *);
    auto retVal = clSetKernelExecInfo(
        kernelInternals.mockMultiDeviceKernel,               // cl_kernel kernel
        CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_INTEL, // cl_kernel_exec_info param_name
        ptrSizeInBytes,                                      // size_t param_value_size
        &euThreadSetting                                     // const void *param_value
    );
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pClDevice, 0);

    const auto enqueueResult = mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr,
                                                                                            0,
                                                                                            false,
                                                                                            multiDispatchInfo,
                                                                                            0,
                                                                                            nullptr,
                                                                                            nullptr);
    EXPECT_EQ(CL_SUCCESS, enqueueResult);
    EXPECT_NE(getNewKernelArbitrationPolicy(euThreadSetting),
              pDevice->getUltCommandStreamReceiver<FamilyType>().streamProperties.stateComputeMode.threadArbitrationPolicy.value);
    EXPECT_EQ(0, pDevice->getUltCommandStreamReceiver<FamilyType>().streamProperties.stateComputeMode.threadArbitrationPolicy.value);

    mockCmdQ->release();
}

HWTEST_F(EnqueueHandlerTest, givenKernelUsingSyncBufferWhenEnqueuingKernelThenSshIsCorrectlyProgrammed) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    struct MockSyncBufferHandler : SyncBufferHandler {
        using SyncBufferHandler::graphicsAllocation;
    };

    pDevice->allocateSyncBufferHandler();

    size_t offset = 0;
    size_t size = 1;

    size_t sshUsageWithoutSyncBuffer;

    {
        MockKernelWithInternals kernelInternals{*pClDevice, context};
        auto kernel = kernelInternals.mockKernel;
        kernel->initialize();

        auto mockCmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, pClDevice, 0));
        mockCmdQ->enqueueKernel(kernel, 1, &offset, &size, &size, 0, nullptr, nullptr);

        sshUsageWithoutSyncBuffer = mockCmdQ->getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 0).getUsed();
    }

    {
        MockKernelWithInternals kernelInternals{*pClDevice, context};
        kernelInternals.kernelInfo.setSyncBuffer(sizeof(uint8_t), 0, 0);
        constexpr auto bindingTableOffset = sizeof(RENDER_SURFACE_STATE);
        kernelInternals.kernelInfo.setBindingTable(bindingTableOffset, 1);
        kernelInternals.kernelInfo.heapInfo.SurfaceStateHeapSize = sizeof(RENDER_SURFACE_STATE) + sizeof(BINDING_TABLE_STATE);
        auto kernel = kernelInternals.mockKernel;
        kernel->initialize();

        auto bindingTableState = reinterpret_cast<BINDING_TABLE_STATE *>(
            ptrOffset(kernel->getSurfaceStateHeap(), bindingTableOffset));
        bindingTableState->setSurfaceStatePointer(0);

        auto mockCmdQ = clUniquePtr(new MockCommandQueueHw<FamilyType>(context, pClDevice, 0));
        mockCmdQ->enqueueKernel(kernel, 1, &offset, &size, &size, 0, nullptr, nullptr);

        auto &surfaceStateHeap = mockCmdQ->getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 0);
        EXPECT_EQ(sshUsageWithoutSyncBuffer + kernelInternals.kernelInfo.heapInfo.SurfaceStateHeapSize, surfaceStateHeap.getUsed());

        ClHardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(*mockCmdQ);

        auto surfaceState = hwParser.getSurfaceState<FamilyType>(&surfaceStateHeap, 0);
        auto pSyncBufferHandler = static_cast<MockSyncBufferHandler *>(pDevice->syncBufferHandler.get());
        EXPECT_EQ(pSyncBufferHandler->graphicsAllocation->getGpuAddress(), surfaceState->getSurfaceBaseAddress());
    }
}

struct EnqueueHandlerTestBasic : public ::testing::Test {
    template <typename FamilyType>
    std::unique_ptr<MockCommandQueueHw<FamilyType>> setupFixtureAndCreateMockCommandQueue() {
        auto executionEnvironment = platform()->peekExecutionEnvironment();

        device = std::make_unique<MockClDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(nullptr, executionEnvironment, 0u));
        context = std::make_unique<MockContext>(device.get());

        auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);

        auto &ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> &>(mockCmdQ->getGpgpuCommandStreamReceiver());
        ultCsr.taskCount = initialTaskCount;

        mockInternalAllocationStorage = new MockInternalAllocationStorage(ultCsr);
        ultCsr.internalAllocationStorage.reset(mockInternalAllocationStorage);

        return mockCmdQ;
    }

    MockInternalAllocationStorage *mockInternalAllocationStorage = nullptr;
    const uint32_t initialTaskCount = 100;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
};

HWTEST_F(EnqueueHandlerTestBasic, givenEnqueueHandlerWhenCommandIsBlokingThenCompletionStampTaskCountIsPassedToWaitForTaskCountAndCleanAllocationListAsRequiredTaskCount) {
    auto mockCmdQ = setupFixtureAndCreateMockCommandQueue<FamilyType>();
    MockKernelWithInternals kernelInternals(*device, context.get());
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(device.get(), kernel);
    const auto enqueueResult = mockCmdQ->template enqueueHandler<CL_COMMAND_WRITE_BUFFER>(nullptr,
                                                                                          0,
                                                                                          true,
                                                                                          multiDispatchInfo,
                                                                                          0,
                                                                                          nullptr,
                                                                                          nullptr);
    EXPECT_EQ(CL_SUCCESS, enqueueResult);
    EXPECT_EQ(initialTaskCount + 1, mockInternalAllocationStorage->lastCleanAllocationsTaskCount);
}

HWTEST_F(EnqueueHandlerTestBasic, givenBlockedEnqueueHandlerWhenCommandIsBlokingThenCompletionStampTaskCountIsPassedToWaitForTaskCountAndCleanAllocationListAsRequiredTaskCount) {
    auto mockCmdQ = setupFixtureAndCreateMockCommandQueue<FamilyType>();

    MockKernelWithInternals kernelInternals(*device, context.get());
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(device.get(), kernel);

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};

    std::thread t0([&mockCmdQ, &userEvent]() {
        while (!mockCmdQ->isQueueBlocked()) {
        }
        userEvent.setStatus(CL_COMPLETE);
    });
    const auto enqueueResult = mockCmdQ->template enqueueHandler<CL_COMMAND_WRITE_BUFFER>(nullptr,
                                                                                          0,
                                                                                          true,
                                                                                          multiDispatchInfo,
                                                                                          1,
                                                                                          waitlist,
                                                                                          nullptr);
    EXPECT_EQ(CL_SUCCESS, enqueueResult);
    EXPECT_EQ(initialTaskCount + 1, mockInternalAllocationStorage->lastCleanAllocationsTaskCount);

    t0.join();
}

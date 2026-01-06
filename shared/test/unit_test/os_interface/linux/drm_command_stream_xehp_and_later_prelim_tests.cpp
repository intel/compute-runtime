/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture_prelim.h"
#include "shared/test/common/os_interface/linux/drm_command_stream_fixture.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_prelim_fixtures.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <sstream>

using namespace NEO;

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenAllocationWithMultipleBufferObjectsWhenMakeResidentIsCalledThenTheBufferObjectsAreMadeResident) {
    if (!FamilyType::supportsCmdSet(IGFX_XE_HP_CORE)) {
        GTEST_SKIP();
    }

    auto size = 1024u;
    auto bo0 = this->createBO(size);
    auto bo1 = this->createBO(size);
    auto bo2 = this->createBO(size);
    auto bo3 = this->createBO(size);
    BufferObjects bos{bo0, bo1, bo2, bo3};
    auto allocation = new DrmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, bos, nullptr, 0u, size, MemoryPool::localMemory);
    allocation->storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));

    csr->CommandStreamReceiver::makeResident(*allocation);
    csr->processResidency(csr->getResidencyAllocations(), 0u);
    EXPECT_TRUE(isResident<FamilyType>(bo0));
    EXPECT_TRUE(isResident<FamilyType>(bo1));
    EXPECT_TRUE(isResident<FamilyType>(bo2));
    EXPECT_TRUE(isResident<FamilyType>(bo3));

    csr->makeNonResident(*allocation);
    EXPECT_FALSE(isResident<FamilyType>(bo0));
    EXPECT_FALSE(isResident<FamilyType>(bo1));
    EXPECT_FALSE(isResident<FamilyType>(bo2));
    EXPECT_FALSE(isResident<FamilyType>(bo3));

    mm->freeGraphicsMemory(allocation);
}

struct DrmCommandStreamEnhancedTestDrmPrelim : public DrmCommandStreamEnhancedTemplate<DrmMockCustomPrelim> {
    void SetUp() override {
        debugManager.flags.UseVmBind.set(1u);
        DrmCommandStreamEnhancedTemplate::SetUp();
    }
    void TearDown() override {
        DrmCommandStreamEnhancedTemplate::TearDown();
        dbgState.reset();
    }

    DebugManagerStateRestore restorer;
};

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTestDrmPrelim, givenEnableImmediateVmBindExtSetWhenFlushThenWaitUserFenceIoctlIsCalled) {
    if (!FamilyType::supportsCmdSet(IGFX_XE_HP_CORE)) {
        GTEST_SKIP();
    }
    debugManager.flags.EnableImmediateVmBindExt.set(1);

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    csr->makeResident(*allocation);

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_NE(0u, mock->context.receivedGemWaitUserFence.addr);
    EXPECT_EQ(2u, mock->context.receivedGemWaitUserFence.value);
    EXPECT_EQ(0u, mock->context.receivedGemWaitUserFence.ctxId);
    EXPECT_EQ(DrmPrelimHelper::getGTEWaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.op);
    EXPECT_EQ(DrmPrelimHelper::getSoftWaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.flags);
    EXPECT_EQ(DrmPrelimHelper::getU64WaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.mask);
    EXPECT_EQ(-1, mock->context.receivedGemWaitUserFence.timeout);

    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTestDrmPrelim, givenDirectSubmissionEnabledWhenFlushThenWaitUserFenceIoctlIsCalled) {
    if (!FamilyType::supportsCmdSet(IGFX_XE_HP_CORE)) {
        GTEST_SKIP();
    }

    this->mock->setDirectSubmissionActive(true);

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    auto allocation = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    csr->makeResident(*allocation);

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    EXPECT_NE(0u, mock->context.receivedGemWaitUserFence.addr);
    EXPECT_EQ(2u, mock->context.receivedGemWaitUserFence.value);
    EXPECT_EQ(0u, mock->context.receivedGemWaitUserFence.ctxId);
    EXPECT_EQ(DrmPrelimHelper::getGTEWaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.op);
    EXPECT_EQ(DrmPrelimHelper::getSoftWaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.flags);
    EXPECT_EQ(DrmPrelimHelper::getU64WaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.mask);
    EXPECT_EQ(-1, mock->context.receivedGemWaitUserFence.timeout);

    this->mock->setDirectSubmissionActive(false);

    mm->freeGraphicsMemory(allocation);
    mm->freeGraphicsMemory(commandBuffer);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTestDrmPrelim, givenWaitUserFenceEnabledWhenUseCtxIdSelectedThenExpectNonZeroContextId) {
    if (!FamilyType::supportsCmdSet(IGFX_XE_HP_CORE)) {
        GTEST_SKIP();
    }

    auto osContextLinux = static_cast<const OsContextLinux *>(device->getDefaultEngine().osContext);
    std::vector<uint32_t> &drmCtxIds = const_cast<std::vector<uint32_t> &>(osContextLinux->getDrmContextIds());
    size_t drmCtxSize = drmCtxIds.size();
    for (uint32_t i = 0; i < drmCtxSize; i++) {
        drmCtxIds[i] = 5u + i;
    }

    auto testDrmCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testDrmCsr->useUserFenceWait = true;
    testDrmCsr->activePartitions = static_cast<uint32_t>(drmCtxSize);

    const auto hasFirstSubmission = device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo) ? 1 : 0;
    auto tagPtr = const_cast<TagAddressType *>(testDrmCsr->getTagAddress());
    *tagPtr = hasFirstSubmission;
    uint64_t tagAddress = castToUint64(tagPtr);
    FlushStamp handleToWait = 123;
    testDrmCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(1u, testDrmCsr->waitUserFenceResult.called);
    EXPECT_EQ(123u, testDrmCsr->waitUserFenceResult.waitValue);

    EXPECT_EQ(drmCtxSize, mock->context.gemWaitUserFenceCalled);
    EXPECT_EQ(tagAddress, mock->context.receivedGemWaitUserFence.addr);
    EXPECT_EQ(handleToWait, mock->context.receivedGemWaitUserFence.value);
    EXPECT_NE(0u, mock->context.receivedGemWaitUserFence.ctxId);
    EXPECT_EQ(DrmPrelimHelper::getGTEWaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.op);
    EXPECT_EQ(0u, mock->context.receivedGemWaitUserFence.flags);
    EXPECT_EQ(DrmPrelimHelper::getU64WaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.mask);
    EXPECT_EQ(-1, mock->context.receivedGemWaitUserFence.timeout);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTestDrmPrelim, givenWaitUserFenceEnabledWhenUseCtxIdNotSelectedAndMultiplePartitionsThenExpectZeroContextIdAndEqualWaitCalls) {
    if (!FamilyType::supportsCmdSet(IGFX_XE_HP_CORE)) {
        GTEST_SKIP();
    }

    auto osContextLinux = static_cast<const OsContextLinux *>(device->getDefaultEngine().osContext);
    std::vector<uint32_t> &drmCtxIds = const_cast<std::vector<uint32_t> &>(osContextLinux->getDrmContextIds());
    size_t drmCtxSize = drmCtxIds.size();
    for (uint32_t i = 0; i < drmCtxSize; i++) {
        drmCtxIds[i] = 5u + i;
    }

    auto testDrmCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testDrmCsr->useUserFenceWait = true;
    testDrmCsr->activePartitions = 2u;
    EXPECT_NE(0u, testDrmCsr->immWritePostSyncWriteOffset);

    auto rootExecEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    auto &gfxCoreHelper = rootExecEnvironment->getHelper<GfxCoreHelper>();
    auto hwInfo = rootExecEnvironment->getHardwareInfo();

    auto osContext = std::make_unique<OsContextLinux>(*mock, rootDeviceIndex, 0,
                                                      EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*rootExecEnvironment)[0],
                                                                                                   PreemptionHelper::getDefaultPreemptionMode(*hwInfo), DeviceBitfield(3)));

    osContext->ensureContextInitialized(false);
    osContext->incRefInternal();

    device->getMemoryManager()->unregisterEngineForCsr(testDrmCsr);

    device->allEngines[0].osContext = osContext.get();

    testDrmCsr->setupContext(*osContext);

    auto tagPtr = testDrmCsr->getTagAddress();
    *tagPtr = 0;

    tagPtr = ptrOffset(tagPtr, testDrmCsr->immWritePostSyncWriteOffset);
    *tagPtr = 0;

    uint64_t tagAddress = castToUint64(const_cast<TagAddressType *>(testDrmCsr->getTagAddress()));
    FlushStamp handleToWait = 123;
    testDrmCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(1u, testDrmCsr->waitUserFenceResult.called);
    EXPECT_EQ(123u, testDrmCsr->waitUserFenceResult.waitValue);

    EXPECT_EQ(2u, mock->context.gemWaitUserFenceCalled);
    EXPECT_EQ(tagAddress + testDrmCsr->immWritePostSyncWriteOffset, mock->context.receivedGemWaitUserFence.addr);
    EXPECT_EQ(handleToWait, mock->context.receivedGemWaitUserFence.value);
    EXPECT_NE(0u, mock->context.receivedGemWaitUserFence.ctxId);
    EXPECT_EQ(DrmPrelimHelper::getGTEWaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.op);
    EXPECT_EQ(0u, mock->context.receivedGemWaitUserFence.flags);
    EXPECT_EQ(DrmPrelimHelper::getU64WaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.mask);
    EXPECT_EQ(-1, mock->context.receivedGemWaitUserFence.timeout);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTestDrmPrelim, givenExternalInterruptIdWhenWaitingTheExecuteFenceWaitOnce) {
    if (!FamilyType::supportsCmdSet(IGFX_XE_HP_CORE)) {
        GTEST_SKIP();
    }

    auto osContextLinux = static_cast<const OsContextLinux *>(device->getDefaultEngine().osContext);
    std::vector<uint32_t> &drmCtxIds = const_cast<std::vector<uint32_t> &>(osContextLinux->getDrmContextIds());
    size_t drmCtxSize = drmCtxIds.size();
    for (uint32_t i = 0; i < drmCtxSize; i++) {
        drmCtxIds[i] = 5u + i;
    }

    auto testDrmCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testDrmCsr->useUserFenceWait = true;
    testDrmCsr->activePartitions = 2u;
    EXPECT_NE(0u, testDrmCsr->immWritePostSyncWriteOffset);
    EXPECT_TRUE(testDrmCsr->waitUserFenceSupported());

    auto rootExecEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    auto &gfxCoreHelper = rootExecEnvironment->getHelper<GfxCoreHelper>();
    auto hwInfo = rootExecEnvironment->getHardwareInfo();

    auto osContext = std::make_unique<OsContextLinux>(*mock, rootDeviceIndex, 0,
                                                      EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*rootExecEnvironment)[0],
                                                                                                   PreemptionHelper::getDefaultPreemptionMode(*hwInfo), DeviceBitfield(3)));

    osContext->ensureContextInitialized(false);
    osContext->incRefInternal();

    device->getMemoryManager()->unregisterEngineForCsr(testDrmCsr);

    device->allEngines[0].osContext = osContext.get();

    testDrmCsr->setupContext(*osContext);

    auto tagPtr = testDrmCsr->getTagAddress();
    *tagPtr = 0;

    tagPtr = ptrOffset(tagPtr, testDrmCsr->immWritePostSyncWriteOffset);
    *tagPtr = 0;

    uint64_t tagAddress = castToUint64(const_cast<TagAddressType *>(testDrmCsr->getTagAddress()));

    EXPECT_EQ(0u, mock->context.gemWaitUserFenceCalled);

    testDrmCsr->waitUserFence(123, tagAddress, 1, true, NEO::InterruptId::notUsed, nullptr);
    EXPECT_EQ(2u, mock->context.gemWaitUserFenceCalled);

    testDrmCsr->waitUserFence(123, tagAddress, 1, true, 0x678, nullptr);
    EXPECT_EQ(3u, mock->context.gemWaitUserFenceCalled);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTestDrmPrelim, givenFailingIoctlWhenWaitingThenDoEarlyReturn) {
    if (!FamilyType::supportsCmdSet(IGFX_XE_HP_CORE)) {
        GTEST_SKIP();
    }

    auto testDrmCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(csr);
    testDrmCsr->useUserFenceWait = true;
    testDrmCsr->activePartitions = 3u;

    auto rootExecEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    auto &gfxCoreHelper = rootExecEnvironment->getHelper<GfxCoreHelper>();
    auto hwInfo = rootExecEnvironment->getHardwareInfo();

    auto osContext = std::make_unique<OsContextLinux>(*mock, rootDeviceIndex, 0,
                                                      EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*rootExecEnvironment)[0],
                                                                                                   PreemptionHelper::getDefaultPreemptionMode(*hwInfo), DeviceBitfield(7)));

    osContext->ensureContextInitialized(false);
    osContext->incRefInternal();

    device->getMemoryManager()->unregisterEngineForCsr(testDrmCsr);

    device->allEngines[0].osContext = osContext.get();

    testDrmCsr->setupContext(*osContext);

    mock->waitUserFenceCall.failSpecificCall = 2;

    FlushStamp handleToWait = 123;

    EXPECT_EQ(0u, mock->waitUserFenceCall.called);

    auto tagPtr = testDrmCsr->getTagAddress();
    *tagPtr = 0;

    tagPtr = ptrOffset(tagPtr, testDrmCsr->immWritePostSyncWriteOffset);
    *tagPtr = 0;

    tagPtr = ptrOffset(tagPtr, testDrmCsr->immWritePostSyncWriteOffset);
    *tagPtr = 0;

    testDrmCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(2u, mock->waitUserFenceCall.called);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTest, givenAllocationWithMultipleBufferObjectsAndTileInstancedSetWhenMakeResidentIsCalledThenTheBufferObjectForDeviceCsrIsMadeResident) {
    if (!FamilyType::supportsCmdSet(IGFX_XE_HP_CORE)) {
        GTEST_SKIP();
    }
    auto size = 1024u;
    auto bo0 = this->createBO(size);
    auto bo1 = this->createBO(size);
    auto bo2 = this->createBO(size);
    auto bo3 = this->createBO(size);
    BufferObjects bos{bo0, bo1, bo2, bo3};
    auto allocation = new DrmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, bos, nullptr, 0u, size, MemoryPool::localMemory);
    allocation->storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocation->storageInfo.tileInstanced = true;

    csr->CommandStreamReceiver::makeResident(*allocation);
    csr->processResidency(csr->getResidencyAllocations(), 0u);
    EXPECT_TRUE(isResident<FamilyType>(bo0));
    EXPECT_FALSE(isResident<FamilyType>(bo1));
    EXPECT_FALSE(isResident<FamilyType>(bo2));
    EXPECT_FALSE(isResident<FamilyType>(bo3));

    csr->processResidency(csr->getResidencyAllocations(), 2u);
    EXPECT_TRUE(isResident<FamilyType>(bo0));
    EXPECT_FALSE(isResident<FamilyType>(bo1));
    EXPECT_TRUE(isResident<FamilyType>(bo2));
    EXPECT_FALSE(isResident<FamilyType>(bo3));

    csr->makeNonResident(*allocation);
    EXPECT_FALSE(isResident<FamilyType>(bo0));
    EXPECT_FALSE(isResident<FamilyType>(bo1));
    EXPECT_FALSE(isResident<FamilyType>(bo2));
    EXPECT_FALSE(isResident<FamilyType>(bo3));

    mm->freeGraphicsMemory(allocation);
}

class DrmCommandStreamForceTileTest : public ::testing::Test {
  public:
    template <typename GfxFamily>
    class MockDrmCommandStreamReceiver : public DrmCommandStreamReceiver<GfxFamily> {
      public:
        ~MockDrmCommandStreamReceiver() override {
        }
        MockDrmCommandStreamReceiver(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex,
                                     DeviceBitfield deviceBitfield,
                                     uint32_t inputHandleId)
            : DrmCommandStreamReceiver<GfxFamily>(executionEnvironment, rootDeviceIndex, deviceBitfield), expectedHandleId(inputHandleId) {
        }

        SubmissionStatus processResidency(ResidencyContainer &allocationsForResidency, uint32_t handleId) override {
            EXPECT_EQ(handleId, expectedHandleId);
            return DrmCommandStreamReceiver<GfxFamily>::processResidency(allocationsForResidency, handleId);
        }

        const uint32_t expectedHandleId = std::numeric_limits<uint32_t>::max();
    };
    template <typename GfxFamily>
    void setUpT() {
        debugManager.flags.EnableL3FlushAfterPostSync.set(0);

        mock = new DrmMock(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);

        auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
        mock->setupIoctlHelper(hwInfo->platform.eProductFamily);
        executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);
        executionEnvironment.rootDeviceEnvironments[0]->initGmm();

        mock->createVirtualMemoryAddressSpace(GfxCoreHelper::getSubDevicesCount(hwInfo));

        auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
        osContext = std::make_unique<OsContextLinux>(*mock, rootDeviceIndex, 0,
                                                     EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[0])[0],
                                                                                                  PreemptionHelper::getDefaultPreemptionMode(*hwInfo), DeviceBitfield(3)));
        osContext->ensureContextInitialized(false);

        csr = new MockDrmCommandStreamReceiver<GfxFamily>(executionEnvironment,
                                                          rootDeviceIndex,
                                                          3,
                                                          expectedHandleId);
        ASSERT_NE(nullptr, csr);
        csr->setupContext(*osContext);

        memoryManager = new DrmMemoryManager(GemCloseWorkerMode::gemCloseWorkerActive,
                                             debugManager.flags.EnableForcePin.get(),
                                             true,
                                             executionEnvironment);
        executionEnvironment.memoryManager.reset(memoryManager);

        // assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
    }

    template <typename GfxFamily>
    void tearDownT() {
        memoryManager->waitForDeletions();
        if (memoryManager->peekGemCloseWorker()) {
            memoryManager->peekGemCloseWorker()->close(true);
        }
        delete csr;
        // Expect 2 calls with DRM_IOCTL_I915_GEM_CONTEXT_DESTROY request on OsContextLinux destruction
        // Expect 1 call with DRM_IOCTL_GEM_CLOSE request on BufferObject close
        mock->expectedIoctlCallsOnDestruction = mock->ioctlCallsCount + 3 + static_cast<uint32_t>(mock->virtualMemoryIds.size());
        mock->expectIoctlCallsOnDestruction = true;
    }

    const uint32_t rootDeviceIndex = 0u;
    const uint32_t expectedHandleId = 1u;

    DebugManagerStateRestore restorer;
    CommandStreamReceiver *csr = nullptr;
    DrmMemoryManager *memoryManager = nullptr;
    DrmMock *mock = nullptr;
    const int mockFd = 33;
    static const uint64_t alignment = MemoryConstants::allocationAlignment;
    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<OsContextLinux> osContext;
};

HWTEST_TEMPLATED_F(DrmCommandStreamForceTileTest, givenForceExecutionTileThenCorrectHandleIdIsSet) {
    if (!FamilyType::supportsCmdSet(IGFX_XE_HP_CORE)) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceExecutionTile.set(expectedHandleId);

    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenPrintIndicesEnabledWhenFlushThenPrintIndicesAndContextInfo) {
    if (!FamilyType::supportsCmdSet(IGFX_XE_HP_CORE)) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintDeviceAndEngineIdOnSubmission.set(true);

    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    StreamCapture capture;
    capture.captureStdout();
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    const std::string engineType = EngineHelpers::engineTypeToString(csr->getOsContext().getEngineType());
    const std::string engineUsage = EngineHelpers::engineUsageToString(csr->getOsContext().getEngineUsage());
    std::ostringstream expectedValue;
    expectedValue << SysCalls::getProcessId() << ": Submission to RootDevice Index: " << csr->getRootDeviceIndex()
                  << ", Sub-Devices Mask: " << csr->getOsContext().getDeviceBitfield().to_ulong()
                  << ", EngineId: " << csr->getOsContext().getEngineType()
                  << " (" << engineType << ", " << engineUsage << ")\n";
    auto &drmContextIds = static_cast<const OsContextLinux *>(&csr->getOsContext())->getDrmContextIds();
    for (uint32_t contextIndex = 0; contextIndex < drmContextIds.size(); contextIndex++) {
        expectedValue << SysCalls::getProcessId() << ": Drm Submission of contextIndex: " << contextIndex << ", with context id " << drmContextIds[contextIndex] << "\n";
    }
    EXPECT_STREQ(capture.getCapturedStdout().c_str(), expectedValue.str().c_str());
}

struct DrmImplicitScalingCommandStreamTest : ::testing::Test {
    void SetUp() override {
        debugManager.flags.EnableL3FlushAfterPostSync.set(0);

        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);

        hwInfo.reset(new HardwareInfo(*defaultHwInfo));
        hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = true;
        hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 0b11;
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(hwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        constexpr int mockFd = 33;
        drm = new DrmMock(mockFd, *executionEnvironment->rootDeviceEnvironments[0]);
        drm->setupIoctlHelper(hwInfo->platform.eProductFamily);
        drm->createVirtualMemoryAddressSpace(GfxCoreHelper::getSubDevicesCount(hwInfo.get()));
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(hwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
        executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
        osContext = std::make_unique<OsContextLinux>(*drm, 0, 0u,
                                                     EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment->rootDeviceEnvironments[0])[0],
                                                                                                  PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), DeviceBitfield(0b11)));
        osContext->ensureContextInitialized(false);

        memoryManager = new DrmMemoryManager(GemCloseWorkerMode::gemCloseWorkerActive, debugManager.flags.EnableForcePin.get(), true, *executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
    }

    void TearDown() override {
        // Expect 2 calls with DRM_IOCTL_I915_GEM_CONTEXT_DESTROY request on OsContextLinux destruction
        // Expect 1 call with DRM_IOCTL_GEM_CLOSE request on BufferObject close
        drm->expectedIoctlCallsOnDestruction = drm->ioctlCallsCount + 3 + static_cast<uint32_t>(drm->virtualMemoryIds.size());
        drm->expectIoctlCallsOnDestruction = true;
    }

    template <typename FamilyType>
    std::unique_ptr<DrmCommandStreamReceiver<FamilyType>> createCsr() {
        auto csr = std::make_unique<DrmCommandStreamReceiver<FamilyType>>(*executionEnvironment, 0, 0b11);
        csr->setupContext(*osContext);
        return csr;
    }

    DrmMock *drm = nullptr;
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<OsContextLinux> osContext;
    DrmMemoryManager *memoryManager;
    std::unique_ptr<HardwareInfo> hwInfo;
    DebugManagerStateRestore restorer;
};

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmImplicitScalingCommandStreamTest, givenTwoTilesWhenFlushIsCalledThenExecIsExecutedOnEveryTile) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    auto csr = createCsr<FamilyType>();

    auto size = 1024u;
    auto multiStorageBo0 = new BufferObject(0u, drm, 3, 30, 0, 1);
    auto multiStorageBo1 = new BufferObject(0u, drm, 3, 31, 0, 1);
    BufferObjects multiStorageBos{multiStorageBo0, multiStorageBo1};
    auto multiStorageAllocation = new DrmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, multiStorageBos, nullptr, 0u, size, MemoryPool::localMemory);
    multiStorageAllocation->storageInfo.memoryBanks = 0b11;
    csr->CommandStreamReceiver::makeResident(*multiStorageAllocation);

    auto tileInstancedBo0 = new BufferObject(0u, drm, 3, 40, 0, 1);
    auto tileInstancedBo1 = new BufferObject(0u, drm, 3, 41, 0, 1);
    BufferObjects tileInstancedBos{tileInstancedBo0, tileInstancedBo1};
    auto tileInstancedAllocation = new DrmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, tileInstancedBos, nullptr, 0u, size, MemoryPool::localMemory);
    tileInstancedAllocation->storageInfo.memoryBanks = 0b11;
    tileInstancedAllocation->storageInfo.tileInstanced = true;
    csr->CommandStreamReceiver::makeResident(*tileInstancedAllocation);

    auto &cs = csr->getCS();
    auto commandBuffer = static_cast<DrmAllocation *>(cs.getGraphicsAllocation());
    ASSERT_NE(nullptr, commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);

    const std::array<NEO::BufferObject *, 4> exec0Bos = {multiStorageBo0, multiStorageBo1, tileInstancedBo0, commandBuffer->getBO()};
    const std::array<NEO::BufferObject *, 4> exec1Bos = {multiStorageBo0, multiStorageBo1, tileInstancedBo1, commandBuffer->getBO()};
    const std::array<NEO::BufferObject *, 8> execBos = {multiStorageBo0, multiStorageBo1, tileInstancedBo0, commandBuffer->getBO(),
                                                        multiStorageBo0, multiStorageBo1, tileInstancedBo1, commandBuffer->getBO()};

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    auto availableSpacePriorToFlush = cs.getAvailableSpace();
    csr->flush(batchBuffer, csr->getResidencyAllocations());
    EXPECT_EQ(availableSpacePriorToFlush, cs.getAvailableSpace());

    EXPECT_EQ(2, drm->ioctlCount.execbuffer2);

    EXPECT_EQ(exec0Bos.size(), drm->execBuffers[0].getBufferCount());
    EXPECT_EQ(exec1Bos.size(), drm->execBuffers[1].getBufferCount());

    for (size_t i = 0; i < execBos.size(); i++) {
        EXPECT_EQ(static_cast<uint32_t>(execBos[i]->peekHandle()), drm->receivedBos[i].getHandle());
    }

    memoryManager->freeGraphicsMemory(tileInstancedAllocation);
    memoryManager->freeGraphicsMemory(multiStorageAllocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmImplicitScalingCommandStreamTest, whenForceExecutionTileIsSetAndEnableWalkerPartitionIsSetButCsrhasOneOsContextThenTileIsNotForced) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceExecutionTile.set(1);
    debugManager.flags.EnableWalkerPartition.set(0);

    struct MockCsr : DrmCommandStreamReceiver<FamilyType> {
        using DrmCommandStreamReceiver<FamilyType>::DrmCommandStreamReceiver;
        int exec(const BatchBuffer &batchBuffer, uint32_t vmHandleId, uint32_t drmContextId, uint32_t index) override {
            EXPECT_EQ(0u, execCalled);
            EXPECT_EQ(0u, vmHandleId);
            EXPECT_EQ(0u, drmContextId);
            EXPECT_EQ(40, this->residency[0]->peekHandle());
            execCalled++;
            return 0;
        }

        uint32_t execCalled = 0;
    };

    auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto osContext = std::make_unique<OsContextLinux>(*drm, 0, 0u,
                                                      EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment->rootDeviceEnvironments[0])[0],
                                                                                                   PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
    osContext->ensureContextInitialized(false);
    auto csr = std::make_unique<MockCsr>(*executionEnvironment, 0, osContext->getDeviceBitfield());
    csr->setupContext(*osContext);

    auto tileInstancedBo0 = new BufferObject(0u, drm, 3, 40, 0, 1);
    auto tileInstancedBo1 = new BufferObject(0u, drm, 3, 41, 0, 1);
    auto tileInstancedBo2 = new BufferObject(0u, drm, 3, 42, 0, 1);
    auto tileInstancedBo3 = new BufferObject(0u, drm, 3, 43, 0, 1);
    BufferObjects tileInstancedBos{tileInstancedBo0, tileInstancedBo1, tileInstancedBo2, tileInstancedBo3};
    auto tileInstancedAllocation = new DrmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, tileInstancedBos, nullptr, 0u, 1024u, MemoryPool::localMemory);
    tileInstancedAllocation->storageInfo.memoryBanks = 0b11;
    tileInstancedAllocation->storageInfo.tileInstanced = true;
    csr->CommandStreamReceiver::makeResident(*tileInstancedAllocation);

    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    memoryManager->freeGraphicsMemory(tileInstancedAllocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmImplicitScalingCommandStreamTest, whenForceExecutionTileIsSetAndEnableWalkerPartitionIsSetThenExecIsCalledWithGoodArguments) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceExecutionTile.set(1);
    debugManager.flags.EnableWalkerPartition.set(0);
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    struct MockCsr : DrmCommandStreamReceiver<FamilyType> {
        using DrmCommandStreamReceiver<FamilyType>::DrmCommandStreamReceiver;
        int exec(const BatchBuffer &batchBuffer, uint32_t vmHandleId, uint32_t drmContextId, uint32_t index) override {
            EXPECT_EQ(0u, execCalled);
            EXPECT_EQ(1u, vmHandleId);
            EXPECT_EQ(0u, drmContextId);
            EXPECT_EQ(41, this->residency[0]->peekHandle());
            execCalled++;
            return 0;
        }

        uint32_t execCalled = 0;
    };
    auto csr = std::make_unique<MockCsr>(*executionEnvironment, 0, osContext->getDeviceBitfield());
    csr->setupContext(*osContext);

    auto tileInstancedBo0 = new BufferObject(0u, drm, 3, 40, 0, 1);
    auto tileInstancedBo1 = new BufferObject(0u, drm, 3, 41, 0, 1);
    auto tileInstancedBo2 = new BufferObject(0u, drm, 3, 42, 0, 1);
    auto tileInstancedBo3 = new BufferObject(0u, drm, 3, 43, 0, 1);
    BufferObjects tileInstancedBos{tileInstancedBo0, tileInstancedBo1, tileInstancedBo2, tileInstancedBo3};
    auto tileInstancedAllocation = new DrmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, tileInstancedBos, nullptr, 0u, 1024u, MemoryPool::localMemory);
    tileInstancedAllocation->storageInfo.memoryBanks = 0b11;
    tileInstancedAllocation->storageInfo.tileInstanced = true;
    csr->CommandStreamReceiver::makeResident(*tileInstancedAllocation);

    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    memoryManager->freeGraphicsMemory(tileInstancedAllocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmImplicitScalingCommandStreamTest, givenDisabledImplicitScalingWhenFlushingThenUseOnlyOneContext) {
    DebugManagerStateRestore debugRestore{};
    debugManager.flags.EnableWalkerPartition.set(0);
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    struct MockCsr : DrmCommandStreamReceiver<FamilyType> {
        using DrmCommandStreamReceiver<FamilyType>::DrmCommandStreamReceiver;
        int exec(const BatchBuffer &batchBuffer, uint32_t vmHandleId, uint32_t drmContextId, uint32_t index) override {
            EXPECT_EQ(0u, execCalled);
            EXPECT_EQ(0u, drmContextId);
            EXPECT_EQ(0u, vmHandleId);
            execCalled++;
            return 0;
        }
        SubmissionStatus processResidency(ResidencyContainer &inputAllocationsForResidency, uint32_t handleId) override {
            EXPECT_EQ(0u, processResidencyCalled);
            EXPECT_EQ(0u, handleId);
            processResidencyCalled++;
            return SubmissionStatus::success;
        }

        uint32_t execCalled = 0;
        uint32_t processResidencyCalled = 0;
    };
    auto csr = std::make_unique<MockCsr>(*executionEnvironment, 0, osContext->getDeviceBitfield());
    csr->setupContext(*osContext);

    const auto size = 1024u;
    BufferObject *bufferObject = new BufferObject(0u, drm, 3, 30, 0, 1);
    BufferObjects bufferObjects{bufferObject};
    auto allocation = new DrmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, bufferObjects, nullptr, 0u, size, MemoryPool::localMemory);
    csr->CommandStreamReceiver::makeResident(*allocation);

    auto &cs = csr->getCS();
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    memoryManager->freeGraphicsMemory(allocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmImplicitScalingCommandStreamTest, givenMultiTileCsrWhenFlushThenVmHandleIdEqualsTileId) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);
    struct MockCsr : DrmCommandStreamReceiver<FamilyType> {
        using DrmCommandStreamReceiver<FamilyType>::DrmCommandStreamReceiver;
        int exec(const BatchBuffer &batchBuffer, uint32_t vmHandleId, uint32_t drmContextId, uint32_t index) override {
            EXPECT_EQ(execCalled, vmHandleId);
            EXPECT_EQ(static_cast<OsContextLinux *>(this->osContext)->getDrmContextIds()[execCalled], drmContextId);
            execCalled++;
            return 0;
        }
        SubmissionStatus processResidency(ResidencyContainer &inputAllocationsForResidency, uint32_t handleId) override {
            EXPECT_EQ(execCalled, handleId);
            return SubmissionStatus::success;
        }

        uint32_t execCalled = 0;
    };
    auto csr = std::make_unique<MockCsr>(*executionEnvironment, 0, osContext->getDeviceBitfield());
    csr->setupContext(*osContext);

    const auto size = 1024u;
    BufferObject *bufferObject = new BufferObject(0u, drm, 3, 30, 0, 1);
    BufferObjects bufferObjects{bufferObject};
    auto allocation = new DrmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, bufferObjects, nullptr, 0u, size, MemoryPool::localMemory);
    csr->CommandStreamReceiver::makeResident(*allocation);

    auto &cs = csr->getCS();
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    memoryManager->freeGraphicsMemory(allocation);
}

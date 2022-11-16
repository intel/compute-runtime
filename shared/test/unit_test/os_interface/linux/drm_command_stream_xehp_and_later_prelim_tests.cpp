/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
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
    auto allocation = new DrmAllocation(0, AllocationType::UNKNOWN, bos, nullptr, 0u, size, MemoryPool::LocalMemory);
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
        DebugManager.flags.UseVmBind.set(1u);
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
    DebugManager.flags.EnableImmediateVmBindExt.set(1);

    auto commandBuffer = mm->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

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
    testDrmCsr->useContextForUserFenceWait = true;
    testDrmCsr->activePartitions = static_cast<uint32_t>(drmCtxSize);

    uint64_t tagAddress = castToUint64(const_cast<uint32_t *>(testDrmCsr->getTagAddress()));
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
    EXPECT_EQ(DrmPrelimHelper::getU32WaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.mask);
    EXPECT_EQ(-1, mock->context.receivedGemWaitUserFence.timeout);
}

HWTEST_TEMPLATED_F(DrmCommandStreamEnhancedTestDrmPrelim, givenWaitUserFenceEnabledWhenUseCtxIdNotSelectedThenExpectZeroContextId) {
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
    testDrmCsr->useContextForUserFenceWait = false;

    uint64_t tagAddress = castToUint64(const_cast<uint32_t *>(testDrmCsr->getTagAddress()));
    FlushStamp handleToWait = 123;
    testDrmCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(1u, testDrmCsr->waitUserFenceResult.called);
    EXPECT_EQ(123u, testDrmCsr->waitUserFenceResult.waitValue);

    EXPECT_EQ(1u, mock->context.gemWaitUserFenceCalled);
    EXPECT_EQ(tagAddress, mock->context.receivedGemWaitUserFence.addr);
    EXPECT_EQ(handleToWait, mock->context.receivedGemWaitUserFence.value);
    EXPECT_EQ(0u, mock->context.receivedGemWaitUserFence.ctxId);
    EXPECT_EQ(DrmPrelimHelper::getGTEWaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.op);
    EXPECT_EQ(0u, mock->context.receivedGemWaitUserFence.flags);
    EXPECT_EQ(DrmPrelimHelper::getU32WaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.mask);
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
    testDrmCsr->useContextForUserFenceWait = false;
    testDrmCsr->activePartitions = 2u;
    EXPECT_NE(0u, testDrmCsr->postSyncWriteOffset);

    uint64_t tagAddress = castToUint64(const_cast<uint32_t *>(testDrmCsr->getTagAddress()));
    FlushStamp handleToWait = 123;
    testDrmCsr->waitForFlushStamp(handleToWait);

    EXPECT_EQ(1u, testDrmCsr->waitUserFenceResult.called);
    EXPECT_EQ(123u, testDrmCsr->waitUserFenceResult.waitValue);

    EXPECT_EQ(2u, mock->context.gemWaitUserFenceCalled);
    EXPECT_EQ(tagAddress + testDrmCsr->postSyncWriteOffset, mock->context.receivedGemWaitUserFence.addr);
    EXPECT_EQ(handleToWait, mock->context.receivedGemWaitUserFence.value);
    EXPECT_EQ(0u, mock->context.receivedGemWaitUserFence.ctxId);
    EXPECT_EQ(DrmPrelimHelper::getGTEWaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.op);
    EXPECT_EQ(0u, mock->context.receivedGemWaitUserFence.flags);
    EXPECT_EQ(DrmPrelimHelper::getU32WaitUserFenceFlag(), mock->context.receivedGemWaitUserFence.mask);
    EXPECT_EQ(-1, mock->context.receivedGemWaitUserFence.timeout);
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
    auto allocation = new DrmAllocation(0, AllocationType::UNKNOWN, bos, nullptr, 0u, size, MemoryPool::LocalMemory);
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
        MockDrmCommandStreamReceiver<GfxFamily>(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex,
                                                DeviceBitfield deviceBitfield,
                                                gemCloseWorkerMode mode, uint32_t inputHandleId)
            : DrmCommandStreamReceiver<GfxFamily>(executionEnvironment, rootDeviceIndex, deviceBitfield, mode), expectedHandleId(inputHandleId) {
        }

        bool processResidency(const ResidencyContainer &allocationsForResidency, uint32_t handleId) override {
            EXPECT_EQ(handleId, expectedHandleId);
            return DrmCommandStreamReceiver<GfxFamily>::processResidency(allocationsForResidency, handleId);
        }

        const uint32_t expectedHandleId = std::numeric_limits<uint32_t>::max();
    };
    template <typename GfxFamily>
    void setUpT() {
        mock = new DrmMock(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);

        auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
        mock->setupIoctlHelper(hwInfo->platform.eProductFamily);
        executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u);
        executionEnvironment.rootDeviceEnvironments[0]->initGmm();

        mock->createVirtualMemoryAddressSpace(HwHelper::getSubDevicesCount(hwInfo));
        osContext = std::make_unique<OsContextLinux>(*mock, rootDeviceIndex, 0,
                                                     EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[0],
                                                                                                  PreemptionHelper::getDefaultPreemptionMode(*hwInfo), DeviceBitfield(3)));
        osContext->ensureContextInitialized();

        csr = new MockDrmCommandStreamReceiver<GfxFamily>(executionEnvironment,
                                                          rootDeviceIndex,
                                                          3,
                                                          gemCloseWorkerMode::gemCloseWorkerActive,
                                                          expectedHandleId);
        ASSERT_NE(nullptr, csr);
        csr->setupContext(*osContext);

        memoryManager = new DrmMemoryManager(gemCloseWorkerMode::gemCloseWorkerActive,
                                             DebugManager.flags.EnableForcePin.get(),
                                             true,
                                             executionEnvironment);
        executionEnvironment.memoryManager.reset(memoryManager);

        //assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
    }

    template <typename GfxFamily>
    void tearDownT() {
        memoryManager->waitForDeletions();
        memoryManager->peekGemCloseWorker()->close(true);
        delete csr;
        // Expect 2 calls with DRM_IOCTL_I915_GEM_CONTEXT_DESTROY request on OsContextLinux destruction
        // Expect 1 call with DRM_IOCTL_GEM_CLOSE request on BufferObject close
        mock->expectedIoctlCallsOnDestruction = mock->ioctlCallsCount + 3 + static_cast<uint32_t>(mock->virtualMemoryIds.size());
        mock->expectIoctlCallsOnDestruction = true;
    }

    const uint32_t rootDeviceIndex = 0u;
    const uint32_t expectedHandleId = 1u;

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
    DebugManager.flags.ForceExecutionTile.set(expectedHandleId);

    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(),
                            0, 0, nullptr, false, false,
                            QueueThrottle::MEDIUM,
                            QueueSliceCount::defaultSliceCount,
                            cs.getUsed(), &cs, nullptr, false};

    csr->flush(batchBuffer, csr->getResidencyAllocations());
}

HWTEST_TEMPLATED_F(DrmCommandStreamTest, givenPrintIndicesEnabledWhenFlushThenPrintIndicesAndContextInfo) {
    if (!FamilyType::supportsCmdSet(IGFX_XE_HP_CORE)) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintDeviceAndEngineIdOnSubmission.set(true);

    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    ::testing::internal::CaptureStdout();
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
    EXPECT_STREQ(::testing::internal::GetCapturedStdout().c_str(), expectedValue.str().c_str());
}

struct DrmImplicitScalingCommandStreamTest : ::testing::Test {
    void SetUp() override {
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);

        hwInfo.reset(new HardwareInfo(*defaultHwInfo));
        hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = true;
        hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 0b11;
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(hwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        constexpr int mockFd = 33;
        drm = new DrmMock(mockFd, *executionEnvironment->rootDeviceEnvironments[0]);
        drm->setupIoctlHelper(hwInfo->platform.eProductFamily);
        drm->createVirtualMemoryAddressSpace(HwHelper::getSubDevicesCount(hwInfo.get()));
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(hwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
        executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        osContext = std::make_unique<OsContextLinux>(*drm, 0, 0u,
                                                     EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[0],
                                                                                                  PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo), DeviceBitfield(0b11)));
        osContext->ensureContextInitialized();

        memoryManager = new DrmMemoryManager(gemCloseWorkerMode::gemCloseWorkerActive, DebugManager.flags.EnableForcePin.get(), true, *executionEnvironment);
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
        auto csr = std::make_unique<DrmCommandStreamReceiver<FamilyType>>(*executionEnvironment, 0, 0b11, gemCloseWorkerMode::gemCloseWorkerActive);
        csr->setupContext(*osContext);
        return csr;
    }

    DrmMock *drm = nullptr;
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<OsContextLinux> osContext;
    DrmMemoryManager *memoryManager;
    std::unique_ptr<HardwareInfo> hwInfo;
};

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmImplicitScalingCommandStreamTest, givenTwoTilesWhenFlushIsCalledThenExecIsExecutedOnEveryTile) {
    auto csr = createCsr<FamilyType>();

    auto size = 1024u;
    auto multiStorageBo0 = new BufferObject(drm, 3, 30, 0, 1);
    auto multiStorageBo1 = new BufferObject(drm, 3, 31, 0, 1);
    BufferObjects multiStorageBos{multiStorageBo0, multiStorageBo1};
    auto multiStorageAllocation = new DrmAllocation(0, AllocationType::UNKNOWN, multiStorageBos, nullptr, 0u, size, MemoryPool::LocalMemory);
    multiStorageAllocation->storageInfo.memoryBanks = 0b11;
    csr->CommandStreamReceiver::makeResident(*multiStorageAllocation);

    auto tileInstancedBo0 = new BufferObject(drm, 3, 40, 0, 1);
    auto tileInstancedBo1 = new BufferObject(drm, 3, 41, 0, 1);
    BufferObjects tileInstancedBos{tileInstancedBo0, tileInstancedBo1};
    auto tileInstancedAllocation = new DrmAllocation(0, AllocationType::UNKNOWN, tileInstancedBos, nullptr, 0u, size, MemoryPool::LocalMemory);
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

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
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
    DebugManager.flags.ForceExecutionTile.set(1);
    DebugManager.flags.EnableWalkerPartition.set(0);

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

    auto osContext = std::make_unique<OsContextLinux>(*drm, 0, 0u,
                                                      EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[0],
                                                                                                   PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
    osContext->ensureContextInitialized();
    auto csr = std::make_unique<MockCsr>(*executionEnvironment, 0, osContext->getDeviceBitfield(),
                                         gemCloseWorkerMode::gemCloseWorkerActive);
    csr->setupContext(*osContext);

    auto tileInstancedBo0 = new BufferObject(drm, 3, 40, 0, 1);
    auto tileInstancedBo1 = new BufferObject(drm, 3, 41, 0, 1);
    auto tileInstancedBo2 = new BufferObject(drm, 3, 42, 0, 1);
    auto tileInstancedBo3 = new BufferObject(drm, 3, 43, 0, 1);
    BufferObjects tileInstancedBos{tileInstancedBo0, tileInstancedBo1, tileInstancedBo2, tileInstancedBo3};
    auto tileInstancedAllocation = new DrmAllocation(0, AllocationType::UNKNOWN, tileInstancedBos, nullptr, 0u, 1024u, MemoryPool::LocalMemory);
    tileInstancedAllocation->storageInfo.memoryBanks = 0b11;
    tileInstancedAllocation->storageInfo.tileInstanced = true;
    csr->CommandStreamReceiver::makeResident(*tileInstancedAllocation);

    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    memoryManager->freeGraphicsMemory(tileInstancedAllocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmImplicitScalingCommandStreamTest, whenForceExecutionTileIsSetAndEnableWalkerPartitionIsSetThenExecIsCalledWithGoodArguments) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceExecutionTile.set(1);
    DebugManager.flags.EnableWalkerPartition.set(0);

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
    auto csr = std::make_unique<MockCsr>(*executionEnvironment, 0, osContext->getDeviceBitfield(),
                                         gemCloseWorkerMode::gemCloseWorkerActive);
    csr->setupContext(*osContext);

    auto tileInstancedBo0 = new BufferObject(drm, 3, 40, 0, 1);
    auto tileInstancedBo1 = new BufferObject(drm, 3, 41, 0, 1);
    auto tileInstancedBo2 = new BufferObject(drm, 3, 42, 0, 1);
    auto tileInstancedBo3 = new BufferObject(drm, 3, 43, 0, 1);
    BufferObjects tileInstancedBos{tileInstancedBo0, tileInstancedBo1, tileInstancedBo2, tileInstancedBo3};
    auto tileInstancedAllocation = new DrmAllocation(0, AllocationType::UNKNOWN, tileInstancedBos, nullptr, 0u, 1024u, MemoryPool::LocalMemory);
    tileInstancedAllocation->storageInfo.memoryBanks = 0b11;
    tileInstancedAllocation->storageInfo.tileInstanced = true;
    csr->CommandStreamReceiver::makeResident(*tileInstancedAllocation);

    auto &cs = csr->getCS();
    CommandStreamReceiverHw<FamilyType>::addBatchBufferEnd(cs, nullptr);
    EncodeNoop<FamilyType>::alignToCacheLine(cs);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    memoryManager->freeGraphicsMemory(tileInstancedAllocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmImplicitScalingCommandStreamTest, givenUseSingleSubdeviceParamSetWhenFlushingThenUseOnlyContext0) {
    struct MockCsr : DrmCommandStreamReceiver<FamilyType> {
        using DrmCommandStreamReceiver<FamilyType>::DrmCommandStreamReceiver;
        int exec(const BatchBuffer &batchBuffer, uint32_t vmHandleId, uint32_t drmContextId, uint32_t index) override {
            EXPECT_EQ(0u, execCalled);
            EXPECT_EQ(0u, drmContextId);
            EXPECT_EQ(0u, vmHandleId);
            execCalled++;
            return 0;
        }
        bool processResidency(const ResidencyContainer &inputAllocationsForResidency, uint32_t handleId) override {
            EXPECT_EQ(0u, processResidencyCalled);
            EXPECT_EQ(0u, handleId);
            processResidencyCalled++;
            return true;
        }

        uint32_t execCalled = 0;
        uint32_t processResidencyCalled = 0;
    };
    auto csr = std::make_unique<MockCsr>(*executionEnvironment, 0, osContext->getDeviceBitfield(),
                                         gemCloseWorkerMode::gemCloseWorkerActive);
    csr->setupContext(*osContext);

    const auto size = 1024u;
    BufferObject *bufferObject = new BufferObject(drm, 3, 30, 0, 1);
    BufferObjects bufferObjects{bufferObject};
    auto allocation = new DrmAllocation(0, AllocationType::UNKNOWN, bufferObjects, nullptr, 0u, size, MemoryPool::LocalMemory);
    csr->CommandStreamReceiver::makeResident(*allocation);

    auto &cs = csr->getCS();
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, true};

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    memoryManager->freeGraphicsMemory(allocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmImplicitScalingCommandStreamTest, givenDisabledImplicitScalingWhenFlushingThenUseOnlyOneContext) {
    DebugManagerStateRestore debugRestore{};
    DebugManager.flags.EnableWalkerPartition.set(0);

    struct MockCsr : DrmCommandStreamReceiver<FamilyType> {
        using DrmCommandStreamReceiver<FamilyType>::DrmCommandStreamReceiver;
        int exec(const BatchBuffer &batchBuffer, uint32_t vmHandleId, uint32_t drmContextId, uint32_t index) override {
            EXPECT_EQ(0u, execCalled);
            EXPECT_EQ(0u, drmContextId);
            EXPECT_EQ(0u, vmHandleId);
            execCalled++;
            return 0;
        }
        bool processResidency(const ResidencyContainer &inputAllocationsForResidency, uint32_t handleId) override {
            EXPECT_EQ(0u, processResidencyCalled);
            EXPECT_EQ(0u, handleId);
            processResidencyCalled++;
            return true;
        }

        uint32_t execCalled = 0;
        uint32_t processResidencyCalled = 0;
    };
    auto csr = std::make_unique<MockCsr>(*executionEnvironment, 0, osContext->getDeviceBitfield(),
                                         gemCloseWorkerMode::gemCloseWorkerActive);
    csr->setupContext(*osContext);

    const auto size = 1024u;
    BufferObject *bufferObject = new BufferObject(drm, 3, 30, 0, 1);
    BufferObjects bufferObjects{bufferObject};
    auto allocation = new DrmAllocation(0, AllocationType::UNKNOWN, bufferObjects, nullptr, 0u, size, MemoryPool::LocalMemory);
    csr->CommandStreamReceiver::makeResident(*allocation);

    auto &cs = csr->getCS();
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    memoryManager->freeGraphicsMemory(allocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DrmImplicitScalingCommandStreamTest, givenMultiTileCsrWhenFlushThenVmHandleIdEqualsTileId) {
    struct MockCsr : DrmCommandStreamReceiver<FamilyType> {
        using DrmCommandStreamReceiver<FamilyType>::DrmCommandStreamReceiver;
        int exec(const BatchBuffer &batchBuffer, uint32_t vmHandleId, uint32_t drmContextId, uint32_t index) override {
            EXPECT_EQ(execCalled, vmHandleId);
            EXPECT_EQ(static_cast<OsContextLinux *>(this->osContext)->getDrmContextIds()[execCalled], drmContextId);
            execCalled++;
            return 0;
        }
        bool processResidency(const ResidencyContainer &inputAllocationsForResidency, uint32_t handleId) override {
            EXPECT_EQ(execCalled, handleId);
            return true;
        }

        uint32_t execCalled = 0;
    };
    auto csr = std::make_unique<MockCsr>(*executionEnvironment, 0, osContext->getDeviceBitfield(),
                                         gemCloseWorkerMode::gemCloseWorkerActive);
    csr->setupContext(*osContext);

    const auto size = 1024u;
    BufferObject *bufferObject = new BufferObject(drm, 3, 30, 0, 1);
    BufferObjects bufferObjects{bufferObject};
    auto allocation = new DrmAllocation(0, AllocationType::UNKNOWN, bufferObjects, nullptr, 0u, size, MemoryPool::LocalMemory);
    csr->CommandStreamReceiver::makeResident(*allocation);

    auto &cs = csr->getCS();
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    csr->flush(batchBuffer, csr->getResidencyAllocations());

    memoryManager->freeGraphicsMemory(allocation);
}

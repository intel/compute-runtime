/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_alloc_dump.h"
#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hardware_context_controller.h"
#include "shared/source/helpers/options.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/fixtures/mock_aub_center_fixture.h"
#include "shared/test/common/fixtures/tbx_command_stream_fixture.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_aub_memory_operations_handler.h"
#include "shared/test/common/mocks/mock_aub_subcapture_manager.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_tbx_csr.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <cstdint>

using namespace NEO;

namespace NEO {
extern TbxCommandStreamReceiverCreateFunc tbxCommandStreamReceiverFactory[IGFX_MAX_CORE];
} // namespace NEO

struct TbxFixture : public TbxCommandStreamFixture,
                    public DeviceFixture,
                    public MockAubCenterFixture {

    using TbxCommandStreamFixture::setUp;

    TbxFixture() : MockAubCenterFixture(CommandStreamReceiverType::tbx) {}

    void setUp() {
        DeviceFixture::setUp();
        setMockAubCenter(*pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]);
        TbxCommandStreamFixture::setUp(pDevice);
        MockAubCenterFixture::setUp();
    }

    void tearDown() {
        MockAubCenterFixture::tearDown();
        TbxCommandStreamFixture::tearDown();
        DeviceFixture::tearDown();
    }
};

using TbxCommandStreamTests = Test<TbxFixture>;
using TbxCommandSteamSimpleTest = TbxCommandStreamTests;

template <typename GfxFamily>
struct MockTbxCsrToTestDumpTbxNonWritable : public TbxCommandStreamReceiverHw<GfxFamily> {
    using TbxCommandStreamReceiverHw<GfxFamily>::TbxCommandStreamReceiverHw;
    using TbxCommandStreamReceiverHw<GfxFamily>::dumpTbxNonWritable;

    bool writeMemory(GraphicsAllocation &gfxAllocation, bool isChunkCopy, uint64_t gpuVaChunkOffset, size_t chunkSize) override {
        return true;
    }
};

TEST(TbxCommandStreamReceiverTest, givenNullFactoryEntryWhenTbxCsrIsCreatedThenNullptrIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    GFXCORE_FAMILY family = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->platform.eRenderCoreFamily;
    VariableBackup<TbxCommandStreamReceiverCreateFunc> tbxCsrFactoryBackup(&tbxCommandStreamReceiverFactory[family]);

    tbxCommandStreamReceiverFactory[family] = nullptr;

    CommandStreamReceiver *csr = TbxCommandStreamReceiver::create("", false, *executionEnvironment, 0, 1);
    EXPECT_EQ(nullptr, csr);
}

TEST(TbxCommandStreamReceiverTest, givenTbxCommandStreamReceiverWhenItIsCreatedWithWrongGfxCoreFamilyThenNullPointerShouldBeReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();

    hwInfo->platform.eRenderCoreFamily = GFXCORE_FAMILY_FORCE_ULONG; // wrong gfx core family

    CommandStreamReceiver *csr = TbxCommandStreamReceiver::create("", false, *executionEnvironment, 0, 1);
    EXPECT_EQ(nullptr, csr);
}

TEST(TbxCommandStreamReceiverTest, givenTbxCommandStreamReceiverWhenTypeIsCheckedThenTbxCsrIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->initializeMemoryManager();
    std::unique_ptr<CommandStreamReceiver> csr(TbxCommandStreamReceiver::create("", false, *executionEnvironment, 0, 1));
    EXPECT_NE(nullptr, csr);
    EXPECT_EQ(CommandStreamReceiverType::tbx, csr->getType());
}

using TbxCommandStreamReceiverHwTest = ::testing::Test;

HWTEST_F(TbxCommandStreamReceiverHwTest, givenTbxCsrWhenHardwareContextIsCreatedThenTbxStreamInCsrIsNotInitialized) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->initializeMemoryManager();
    std::unique_ptr<CommandStreamReceiver> tbxCsr(TbxCommandStreamReceiver::create("", false, *executionEnvironment, 0, 1));
    EXPECT_NE(nullptr, tbxCsr);

    EXPECT_FALSE(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(tbxCsr.get())->streamInitialized);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenMakeResidentIsCalledForGraphicsAllocationThenItShouldPushAllocationForResidencyToCsr) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pCommandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, graphicsAllocation);

    auto currResidencyAllocationsSize = tbxCsr->getResidencyAllocations().size();

    tbxCsr->makeResident(*graphicsAllocation);

    EXPECT_EQ(currResidencyAllocationsSize + 1, tbxCsr->getResidencyAllocations().size());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST(TbxCommandStreamReceiverTest, givenTbxCommandStreamReceiverWhenAskingForSkipResourceCleanupThenReturnTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->initializeMemoryManager();
    std::unique_ptr<CommandStreamReceiver> csr(TbxCommandStreamReceiver::create("", false, *executionEnvironment, 0, 1));
    EXPECT_NE(nullptr, csr);
    EXPECT_TRUE(csr->skipResourceCleanup());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenItIsCreatedWithAubDumpAndAubCaptureFileNameHasBeenSpecifiedThenItShouldBeUsedToOpenTheFileWithAubCapture) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.AUBDumpCaptureFileName.set("aubcapture_file_name.aub");

    using TbxCsrWithAubDump = CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>>;
    std::unique_ptr<TbxCsrWithAubDump> tbxCsrWithAubDump(static_cast<TbxCsrWithAubDump *>(
        TbxCommandStreamReceiverHw<FamilyType>::create("aubfile", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield())));

    EXPECT_TRUE(tbxCsrWithAubDump->aubManager->isOpen());
    EXPECT_STREQ("aubcapture_file_name.aub", tbxCsrWithAubDump->aubManager->getFileName().c_str());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenMakeResidentHasAlreadyBeenCalledForGraphicsAllocationThenItShouldNotPushAllocationForResidencyAgainToCsr) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pCommandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, graphicsAllocation);
    auto currResidencyAllocationsSize = tbxCsr->getResidencyAllocations().size();

    tbxCsr->makeResident(*graphicsAllocation);

    EXPECT_EQ(currResidencyAllocationsSize + 1, tbxCsr->getResidencyAllocations().size());

    tbxCsr->makeResident(*graphicsAllocation);

    EXPECT_EQ(currResidencyAllocationsSize + 1, tbxCsr->getResidencyAllocations().size());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenWriteMemoryIsCalledForGraphicsAllocationWithNonZeroSizeThenItShouldReturnTrue) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    tbxCsr->initializeEngine();
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pCommandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_TRUE(tbxCsr->writeMemory(*graphicsAllocation));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenWriteMemoryIsCalledWithGraphicsAllocationThatIsOnlyOneTimeWriteableThenGraphicsAllocationIsUpdated) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    tbxCsr->initializeEngine();
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pCommandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::buffer, pDevice->getDeviceBitfield()});
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_TRUE(tbxCsr->isTbxWritable(*graphicsAllocation));
    EXPECT_TRUE(tbxCsr->writeMemory(*graphicsAllocation));
    EXPECT_FALSE(tbxCsr->isTbxWritable(*graphicsAllocation));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenWriteMemoryIsCalledWithGraphicsAllocationThatIsOnlyOneTimeWriteableButAlreadyWrittenThenGraphicsAllocationIsNotUpdated) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    tbxCsr->initializeEngine();
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pCommandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::buffer, pDevice->getDeviceBitfield()});
    ASSERT_NE(nullptr, graphicsAllocation);

    tbxCsr->setTbxWritable(false, *graphicsAllocation);
    EXPECT_FALSE(tbxCsr->writeMemory(*graphicsAllocation));
    EXPECT_FALSE(tbxCsr->isTbxWritable(*graphicsAllocation));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenWriteMemoryIsCalledForGraphicsAllocationWithZeroSizeThenItShouldReturnFalse) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    tbxCsr->initializeEngine();
    MockGraphicsAllocation graphicsAllocation((void *)0x1234, 0);

    EXPECT_FALSE(tbxCsr->writeMemory(graphicsAllocation));
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenProcessResidencyIsCalledWithoutAllocationsForResidencyThenItShouldProcessAllocationsFromMemoryManager) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    tbxCsr->initializeEngine();
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pCommandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_FALSE(graphicsAllocation->isResident(tbxCsr->getOsContext().getContextId()));

    ResidencyContainer allocationsForResidency = {graphicsAllocation};
    tbxCsr->processResidency(allocationsForResidency, 0u);

    EXPECT_TRUE(graphicsAllocation->isResident(tbxCsr->getOsContext().getContextId()));
    EXPECT_EQ(tbxCsr->peekTaskCount() + 1, graphicsAllocation->getResidencyTaskCount(tbxCsr->getOsContext().getContextId()));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenProcessResidencyIsCalledWithAllocationsForResidencyThenItShouldProcessGivenAllocations) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    tbxCsr->initializeEngine();
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pCommandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_FALSE(graphicsAllocation->isResident(tbxCsr->getOsContext().getContextId()));

    ResidencyContainer allocationsForResidency = {graphicsAllocation};
    tbxCsr->processResidency(allocationsForResidency, 0u);

    EXPECT_TRUE(graphicsAllocation->isResident(tbxCsr->getOsContext().getContextId()));
    EXPECT_EQ(tbxCsr->peekTaskCount() + 1, graphicsAllocation->getResidencyTaskCount(tbxCsr->getOsContext().getContextId()));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenFlushIsCalledThenItShouldProcessAllocationsForResidency) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pCommandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, graphicsAllocation);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pCommandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    ResidencyContainer allocationsForResidency = {graphicsAllocation};

    EXPECT_FALSE(graphicsAllocation->isResident(tbxCsr->getOsContext().getContextId()));

    tbxCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(graphicsAllocation->isResident(tbxCsr->getOsContext().getContextId()));
    EXPECT_EQ(tbxCsr->peekTaskCount() + 1, graphicsAllocation->getResidencyTaskCount(tbxCsr->getOsContext().getContextId()));

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenFlushIsCalledThenItMakesCommandBufferAllocationsProperlyResident) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pCommandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

    ResidencyContainer allocationsForResidency = {};

    EXPECT_FALSE(commandBuffer->isResident(tbxCsr->getOsContext().getContextId()));

    tbxCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(commandBuffer->isResident(tbxCsr->getOsContext().getContextId()));
    EXPECT_EQ(tbxCsr->peekTaskCount() + 1, commandBuffer->getTaskCount(tbxCsr->getOsContext().getContextId()));
    EXPECT_EQ(tbxCsr->peekTaskCount() + 1, commandBuffer->getResidencyTaskCount(tbxCsr->getOsContext().getContextId()));
    ASSERT_EQ(1u, allocationsForResidency.size());
    EXPECT_EQ(commandBuffer, allocationsForResidency[0]);

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(TbxCommandStreamTests, givenNoDbgDeviceIdFlagWhenTbxCsrIsCreatedThenUseDefaultDeviceId) {
    const HardwareInfo &hwInfo = *defaultHwInfo;
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(pCommandStreamReceiver);
    EXPECT_EQ(hwInfo.capabilityTable.aubDeviceId, tbxCsr->aubDeviceId);
}

HWTEST_F(TbxCommandStreamTests, givenDbgDeviceIdFlagIsSetWhenTbxCsrIsCreatedThenUseDebugDeviceId) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.OverrideAubDeviceId.set(9); // this is Hsw, not used
    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsr(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(TbxCommandStreamReceiver::create("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield())));
    EXPECT_EQ(9u, tbxCsr->aubDeviceId);
}

HWTEST_F(TbxCommandSteamSimpleTest, givenTbxCsrWhenCallingMakeSurfacePackNonResidentThenOnlyResidentAllocationsAddedAllocationsForDownload) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment, pDevice->getDeviceBitfield()};
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    EXPECT_EQ(0u, tbxCsr.allocationsForDownload.size());

    MockGraphicsAllocation allocation1, allocation2, allocation3;
    allocation1.usageInfos[0].residencyTaskCount = 1;
    allocation3.usageInfos[0].residencyTaskCount = 1;
    ASSERT_TRUE(allocation1.isResident(0u));
    ASSERT_FALSE(allocation2.isResident(0u));
    ASSERT_TRUE(allocation3.isResident(0u));

    ResidencyContainer allocationsForResidency{&allocation1, &allocation2, &allocation3};

    tbxCsr.makeSurfacePackNonResident(allocationsForResidency, true);
    std::set<GraphicsAllocation *> expectedAllocationsForDownload = {&allocation1, &allocation3};
    EXPECT_EQ(expectedAllocationsForDownload, tbxCsr.allocationsForDownload);
}

HWTEST_F(TbxCommandSteamSimpleTest, givenTbxCsrAndResidentAllocationWhenProcessResidencyIsCalledThenWriteMemoryIsCalledOnResidentAllocations) {
    auto mockManager = reinterpret_cast<MockAubManager *>(pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter->getAubManager());

    auto memoryOperationsHandler = new NEO::MockAubMemoryOperationsHandler(mockManager);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);

    auto commandStreamReceiver = std::make_unique<MockTbxCsr<FamilyType>>(*pDevice->getExecutionEnvironment(), pDevice->getDeviceBitfield());

    pDevice->getExecutionEnvironment()->memoryManager->reInitLatestContextId();
    auto osContext = pDevice->getExecutionEnvironment()->memoryManager->createAndRegisterOsContext(commandStreamReceiver.get(),
                                                                                                   EngineDescriptorHelper::getDefaultDescriptor({getChosenEngineType(*defaultHwInfo), EngineUsage::regular},
                                                                                                                                                PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
    commandStreamReceiver->setupContext(*osContext);
    commandStreamReceiver->initializeTagAllocation();
    commandStreamReceiver->createGlobalFenceAllocation();

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    ResidencyContainer allocationsForResidency = {&allocation};

    MockGraphicsAllocation allocation2(reinterpret_cast<void *>(0x5000), 0x5000, 0x1000);
    GraphicsAllocation *allocPtr = &allocation2;
    memoryOperationsHandler->makeResident(pDevice, ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false, false);
    EXPECT_TRUE(mockManager->writeMemory2Called);

    mockManager->storeAllocationParams = true;
    commandStreamReceiver->writeMemoryGfxAllocCalled = false;
    mockManager->writeMemory2Called = false;

    commandStreamReceiver->processResidency(allocationsForResidency, 0u);
    EXPECT_TRUE(mockManager->writeMemory2Called);
    EXPECT_TRUE(commandStreamReceiver->writeMemoryGfxAllocCalled);
    ASSERT_EQ(2u, mockManager->storedAllocationParams.size());
    EXPECT_EQ(allocation2.getGpuAddress(), mockManager->storedAllocationParams[1].gfxAddress);
    ASSERT_EQ(1u, memoryOperationsHandler->residentAllocations.size());
    EXPECT_EQ(&allocation2, memoryOperationsHandler->residentAllocations[0]);
}

HWTEST_F(TbxCommandSteamSimpleTest, givenTbxCsrWhenCallingWaitForTaskCountWithKmdNotifyFallbackThenTagAllocationAndScheduledAllocationsAreDownloaded) {
    MockTbxCsrRegisterDownloadedAllocations<FamilyType> tbxCsr{*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()};
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));

    tbxCsr.setupContext(osContext);
    tbxCsr.initializeTagAllocation();
    *tbxCsr.getTagAddress() = 0u;
    tbxCsr.latestFlushedTaskCount = 1u;

    MockGraphicsAllocation allocation1, allocation2, allocation3;
    allocation1.usageInfos[0].residencyTaskCount = 1;
    allocation2.usageInfos[0].residencyTaskCount = 1;
    allocation3.usageInfos[0].residencyTaskCount = 1;
    ASSERT_TRUE(allocation1.isResident(0u));
    ASSERT_TRUE(allocation2.isResident(0u));
    ASSERT_TRUE(allocation3.isResident(0u));

    tbxCsr.allocationsForDownload = {&allocation1, &allocation2, &allocation3};

    tbxCsr.waitForTaskCountWithKmdNotifyFallback(0u, 0u, false, QueueThrottle::MEDIUM);

    std::set<GraphicsAllocation *> expectedDownloadedAllocations = {tbxCsr.getTagAllocation(), &allocation1, &allocation2, &allocation3};
    EXPECT_EQ(expectedDownloadedAllocations, tbxCsr.downloadedAllocations);
    EXPECT_EQ(0u, tbxCsr.allocationsForDownload.size());
}

HWTEST_F(TbxCommandSteamSimpleTest, givenTbxCsrWhenCallingWaitForCompletionWithTimeoutThenFlushIsCalledAndTagAllocationAndScheduledAllocationsAreDownloaded) {
    MockTbxCsrRegisterDownloadedAllocations<FamilyType> tbxCsr{*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()};
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));

    tbxCsr.setupContext(osContext);
    tbxCsr.initializeTagAllocation();
    *tbxCsr.getTagAddress() = 0u;
    tbxCsr.latestFlushedTaskCount = 1u;

    MockGraphicsAllocation allocation1, allocation2, allocation3;
    allocation1.usageInfos[0].residencyTaskCount = 1;
    allocation2.usageInfos[0].residencyTaskCount = 1;
    allocation3.usageInfos[0].residencyTaskCount = 1;
    ASSERT_TRUE(allocation1.isResident(0u));
    ASSERT_TRUE(allocation2.isResident(0u));
    ASSERT_TRUE(allocation3.isResident(0u));

    tbxCsr.allocationsForDownload = {&allocation1, &allocation2, &allocation3};

    tbxCsr.waitForCompletionWithTimeout(WaitParams{false, true, false, 0}, 0);

    std::set<GraphicsAllocation *> expectedDownloadedAllocations = {tbxCsr.getTagAllocation(), &allocation1, &allocation2, &allocation3};
    EXPECT_EQ(expectedDownloadedAllocations, tbxCsr.downloadedAllocations);
    EXPECT_EQ(0u, tbxCsr.allocationsForDownload.size());
    EXPECT_TRUE(tbxCsr.flushBatchedSubmissionsCalled);
}

HWTEST_F(TbxCommandSteamSimpleTest, givenLatestFlushedTaskCountLowerThanTagWhenFlushSubmissionsAndDownloadAllocationsThenFlushTagUpdate) {
    MockTbxCsrRegisterDownloadedAllocations<FamilyType> tbxCsr{*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()};
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));

    tbxCsr.setupContext(osContext);
    tbxCsr.initializeTagAllocation();
    *tbxCsr.getTagAddress() = 1u;
    tbxCsr.latestFlushedTaskCount = 0u;
    EXPECT_FALSE(tbxCsr.flushTagCalled);

    EXPECT_EQ(0u, tbxCsr.obtainUniqueOwnershipCalled);
    tbxCsr.flushSubmissionsAndDownloadAllocations(1u, false);
    EXPECT_EQ(1u, tbxCsr.obtainUniqueOwnershipCalled);

    EXPECT_TRUE(tbxCsr.flushTagCalled);
}

HWTEST_F(TbxCommandSteamSimpleTest, givenTbxCsrWhenDownloadAllocatoinsCalledThenTagAndScheduledAllocationsAreDownloadedAndRemovedFromContainer) {
    MockTbxCsrRegisterDownloadedAllocations<FamilyType> tbxCsr{*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()};
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));

    tbxCsr.setupContext(osContext);
    tbxCsr.initializeTagAllocation();
    *tbxCsr.getTagAddress() = 0u;

    MockGraphicsAllocation allocation1, allocation2, allocation3;
    tbxCsr.allocationsForDownload = {&allocation1, &allocation2, &allocation3};

    allocation1.updateTaskCount(0, tbxCsr.getOsContext().getContextId());
    allocation2.updateTaskCount(0, tbxCsr.getOsContext().getContextId());
    allocation3.updateTaskCount(0, tbxCsr.getOsContext().getContextId());

    EXPECT_EQ(0u, tbxCsr.obtainUniqueOwnershipCalled);
    tbxCsr.downloadAllocations(true);
    EXPECT_EQ(1u, tbxCsr.obtainUniqueOwnershipCalled);

    std::set<GraphicsAllocation *> expectedDownloadedAllocations = {tbxCsr.getTagAllocation(), &allocation1, &allocation2, &allocation3};
    EXPECT_EQ(0u, tbxCsr.allocationsForDownload.size());
}

HWTEST_F(TbxCommandSteamSimpleTest, givenTbxCsrWhenUpdatingTaskCountDuringWaitThenDontRemoveFromContainer) {
    MockTbxCsrRegisterDownloadedAllocations<FamilyType> tbxCsr{*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()};
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));

    tbxCsr.downloadAllocationImpl = nullptr;

    tbxCsr.setupContext(osContext);
    tbxCsr.initializeTagAllocation();

    auto tagAddress = tbxCsr.getTagAddress();

    *tagAddress = 0u;
    tbxCsr.latestFlushedTaskCount = 1;

    MockGraphicsAllocation allocation1, allocation2, allocation3;
    tbxCsr.allocationsForDownload = {&allocation1, &allocation2, &allocation3};

    tbxCsr.makeResident(allocation1);
    tbxCsr.makeResident(allocation2);
    tbxCsr.makeResident(allocation3);

    allocation2.updateTaskCount(2u, tbxCsr.getOsContext().getContextId());

    tbxCsr.downloadAllocations(false);
    EXPECT_EQ(0u, tbxCsr.obtainUniqueOwnershipCalled);
    EXPECT_EQ(3u, tbxCsr.allocationsForDownload.size());

    *tagAddress = 1u;

    tbxCsr.downloadAllocations(false);
    EXPECT_EQ(1u, tbxCsr.obtainUniqueOwnershipCalled);
    EXPECT_EQ(1u, tbxCsr.allocationsForDownload.size());
    EXPECT_NE(tbxCsr.allocationsForDownload.find(&allocation2), tbxCsr.allocationsForDownload.end());
}

HWTEST_F(TbxCommandSteamSimpleTest, givenAllocationWithBiggerTaskCountThanWaitingTaskCountThenDontRemoveFromContainer) {
    MockTbxCsrRegisterDownloadedAllocations<FamilyType> tbxCsr{*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()};
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));

    tbxCsr.setupContext(osContext);
    tbxCsr.initializeTagAllocation();
    *tbxCsr.getTagAddress() = 0u;
    tbxCsr.latestFlushedTaskCount = 1;

    MockGraphicsAllocation allocation1, allocation2, allocation3;
    tbxCsr.allocationsForDownload = {&allocation1, &allocation2, &allocation3};

    tbxCsr.makeResident(allocation1);
    tbxCsr.makeResident(allocation2);
    tbxCsr.makeResident(allocation3);

    auto contextId = tbxCsr.getOsContext().getContextId();

    allocation1.updateTaskCount(2, contextId);
    allocation2.updateTaskCount(1, contextId);
    allocation3.updateTaskCount(2, contextId);

    *tbxCsr.getTagAddress() = 1u;
    tbxCsr.downloadAllocations(false, 1);
    EXPECT_EQ(1u, tbxCsr.obtainUniqueOwnershipCalled);
    EXPECT_EQ(2u, tbxCsr.allocationsForDownload.size());

    EXPECT_NE(tbxCsr.allocationsForDownload.find(&allocation1), tbxCsr.allocationsForDownload.end());
    EXPECT_NE(tbxCsr.allocationsForDownload.find(&allocation3), tbxCsr.allocationsForDownload.end());
}

HWTEST_F(TbxCommandSteamSimpleTest, givenDifferentTaskCountThanLatestFlushedWhenDownloadingThenPickSmallest) {
    MockTbxCsrRegisterDownloadedAllocations<FamilyType> tbxCsr{*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()};
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));

    tbxCsr.downloadAllocationImpl = nullptr;

    tbxCsr.setupContext(osContext);
    tbxCsr.initializeTagAllocation();
    *tbxCsr.getTagAddress() = 0;
    tbxCsr.latestFlushedTaskCount = 1;

    tbxCsr.downloadAllocations(false, 2);
    EXPECT_EQ(0u, tbxCsr.obtainUniqueOwnershipCalled);

    *tbxCsr.getTagAddress() = 1;

    tbxCsr.downloadAllocations(false, 2);
    EXPECT_EQ(1u, tbxCsr.obtainUniqueOwnershipCalled);

    tbxCsr.latestFlushedTaskCount = 3;
    tbxCsr.downloadAllocations(false, 2);
    EXPECT_EQ(1u, tbxCsr.obtainUniqueOwnershipCalled);

    *tbxCsr.getTagAddress() = 3;
    tbxCsr.downloadAllocations(false, 2);
    EXPECT_EQ(2u, tbxCsr.obtainUniqueOwnershipCalled);
}

HWTEST_F(TbxCommandSteamSimpleTest, whenTbxCommandStreamReceiverIsCreatedThenPPGTTAndGGTTCreatedHavePhysicalAddressAllocatorSet) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());

    uintptr_t address = 0x20000;
    auto physicalAddress = tbxCsr.ppgtt->map(address, MemoryConstants::pageSize, 0, MemoryBanks::mainBank);
    EXPECT_NE(0u, physicalAddress);

    physicalAddress = tbxCsr.ggtt->map(address, MemoryConstants::pageSize, 0, MemoryBanks::mainBank);
    EXPECT_NE(0u, physicalAddress);
}

HWTEST_F(TbxCommandSteamSimpleTest, givenTbxCommandStreamReceiverWhenPhysicalAddressAllocatorIsCreatedThenItIsNotNull) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    std::unique_ptr<PhysicalAddressAllocator> allocator(tbxCsr.createPhysicalAddressAllocator(&hardwareInfo, pDevice->getReleaseHelper()));
    ASSERT_NE(nullptr, allocator);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenItIsCreatedWithUseAubStreamFalseThenDontInitializeAubManager) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.UseAubStream.set(false);
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), false, 1);
    executionEnvironment.initializeMemoryManager();
    executionEnvironment.initGmm();

    auto tbxCsr = std::make_unique<TbxCommandStreamReceiverHw<FamilyType>>(executionEnvironment, 0, 1);
    EXPECT_EQ(nullptr, executionEnvironment.rootDeviceEnvironments[0]->aubCenter->getAubManager());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenFlushIsCalledThenItShouldCallTheExpectedHwContextFunctions) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, pDevice->getDeviceBitfield()});

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 1;

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    ResidencyContainer allocationsForResidency = {&allocation};

    tbxCsr.flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(mockHardwareContext->initializeCalled);
    EXPECT_FALSE(mockHardwareContext->writeAndSubmitCalled);
    EXPECT_TRUE(mockHardwareContext->submitCalled);
    EXPECT_FALSE(mockHardwareContext->pollForCompletionCalled);

    EXPECT_TRUE(tbxCsr.writeMemoryWithAubManagerCalled);
    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(TbxCommandStreamTests, whenPollForAubCompletionCalledThenDontInsertPoll) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());

    tbxCsr.pollForAubCompletion();
    EXPECT_FALSE(tbxCsr.pollForCompletionCalled);
    EXPECT_FALSE(mockHardwareContext->pollForCompletionCalled);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverInBatchedModeWhenFlushIsCalledThenItShouldMakeCommandBufferResident) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::batchedDispatch));

    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, pDevice->getDeviceBitfield()});

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    batchBuffer.startOffset = 1;
    ResidencyContainer allocationsForResidency;

    tbxCsr.flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(tbxCsr.writeMemoryWithAubManagerCalled);
    EXPECT_EQ(1u, batchBuffer.commandBufferAllocation->getResidencyTaskCount(tbxCsr.getOsContext().getContextId()));
    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenFlushIsCalledWithZeroSizedBufferThenSubmitIsNotCalledOnHwContext) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, pDevice->getDeviceBitfield()});

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    ResidencyContainer allocationsForResidency;

    tbxCsr.flush(batchBuffer, allocationsForResidency);

    EXPECT_FALSE(mockHardwareContext->submitCalled);
    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenMakeResidentIsCalledThenItShouldCallTheExpectedHwContextFunctions) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);
    tbxCsr.initializeEngine();

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    ResidencyContainer allocationsForResidency = {&allocation};
    tbxCsr.processResidency(allocationsForResidency, 0u);

    EXPECT_TRUE(tbxCsr.writeMemoryWithAubManagerCalled);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenDownloadAllocationIsCalledThenItShouldCallTheExpectedHwContextFunctions) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    tbxCsr.downloadAllocation(allocation);

    EXPECT_TRUE(mockHardwareContext->readMemoryCalled);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenDownloadAllocationIsCalledThenDecanonizeGpuVa) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());

    uint64_t gpuVa = pDevice->getHardwareInfo().capabilityTable.gpuAddressSpace;
    uint64_t canonizedGpuVa = pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getGmmHelper()->canonize(gpuVa);

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1230000), canonizedGpuVa, 0x10000);
    tbxCsr.downloadAllocation(allocation);

    EXPECT_TRUE(mockHardwareContext->readMemoryCalled);
    EXPECT_EQ(gpuVa, mockHardwareContext->latestGpuVaForMemoryRead);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenOsContextIsSetThenCreateHardwareContext) {
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    std::string fileName = "";

    EngineUsage engineUsageModes[] = {EngineUsage::lowPriority, EngineUsage::regular, EngineUsage::highPriority};

    for (auto mode : engineUsageModes) {
        auto engineDescriptor = EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(pDevice->getRootDeviceEnvironment())[0], pDevice->getDeviceBitfield());
        engineDescriptor.engineTypeUsage.second = mode;
        MockOsContext osContext(0, engineDescriptor);

        std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsr(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(TbxCommandStreamReceiver::create(fileName, false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield())));
        EXPECT_EQ(nullptr, tbxCsr->hardwareContextController.get());

        tbxCsr->setupContext(osContext);
        EXPECT_NE(nullptr, tbxCsr->hardwareContextController.get());
    }
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenPollForCompletionImplIsCalledThenSimulatedCsrMethodIsCalled) {
    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsr(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(TbxCommandStreamReceiver::create("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield())));
    tbxCsr->pollForCompletionImpl();
}
HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenItIsQueriedForPreferredTagPoolSizeThenOneIsReturned) {
    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsr(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(TbxCommandStreamReceiver::create("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield())));
    EXPECT_EQ(1u, tbxCsr->getPreferredTagPoolSize());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenCreatedWithAubDumpThenFileNameIsExtendedWithSystemInfo) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment.initializeMemoryManager();
    executionEnvironment.initGmm();

    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    setMockAubCenter(*rootDeviceEnvironment, CommandStreamReceiverType::tbx, true);

    auto fullName = AUBCommandStreamReceiver::createFullFilePath(*defaultHwInfo, "aubfile", mockRootDeviceIndex);

    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsr(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(TbxCommandStreamReceiver::create("aubfile", true, executionEnvironment, 0, 1)));
    EXPECT_STREQ(fullName.c_str(), rootDeviceEnvironment->aubFileNameReceived.c_str());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenCreatedWithAubDumpThenOpenIsCalledOnAubManagerToOpenFileStream) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment.initializeMemoryManager();

    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsrWithAubDump(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(
        TbxCommandStreamReceiver::create("aubfile", true, executionEnvironment, 0, 1)));
    EXPECT_TRUE(tbxCsrWithAubDump->aubManager->isOpen());
}

using SimulatedCsrTest = ::testing::Test;
HWTEST_F(SimulatedCsrTest, givenTbxCsrTypeWhenCreateCommandStreamReceiverThenProperAubCenterIsInitalized) {
    uint32_t expectedRootDeviceIndex = 10;
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, expectedRootDeviceIndex + 2);
    executionEnvironment.initializeMemoryManager();

    auto rootDeviceEnvironment = new MockRootDeviceEnvironment(executionEnvironment);
    executionEnvironment.rootDeviceEnvironments[expectedRootDeviceIndex].reset(rootDeviceEnvironment);
    rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());

    EXPECT_EQ(nullptr, executionEnvironment.rootDeviceEnvironments[expectedRootDeviceIndex]->aubCenter.get());
    EXPECT_FALSE(rootDeviceEnvironment->initAubCenterCalled);

    auto csr = std::make_unique<TbxCommandStreamReceiverHw<FamilyType>>(executionEnvironment, expectedRootDeviceIndex, 1);
    EXPECT_NE(nullptr, executionEnvironment.rootDeviceEnvironments[expectedRootDeviceIndex]->aubCenter.get());
    EXPECT_TRUE(rootDeviceEnvironment->initAubCenterCalled);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenCreatedWithAubDumpInSubCaptureModeThenCreateSubCaptureManagerAndGenerateSubCaptureFileName) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.AUBDumpSubCaptureMode.set(static_cast<int32_t>(AubSubCaptureManager::SubCaptureMode::filter));

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment.initializeMemoryManager();

    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsrWithAubDump(static_cast<TbxCommandStreamReceiverHw<FamilyType> *>(
        TbxCommandStreamReceiver::create("aubfile", true, executionEnvironment, 0, 1)));
    EXPECT_TRUE(tbxCsrWithAubDump->aubManager->isOpen());

    auto subCaptureManager = tbxCsrWithAubDump->subCaptureManager.get();
    EXPECT_NE(nullptr, subCaptureManager);

    EXPECT_STREQ(subCaptureManager->getSubCaptureFileName("kernelName").c_str(), tbxCsrWithAubDump->aubManager->getFileName().c_str());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenCreatedWithAubDumpSeveralTimesThenOpenIsCalledOnAubManagerOnceOnly) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, 1);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment.initializeMemoryManager();

    auto tbxCsrWithAubDump1 = std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>>(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(
        TbxCommandStreamReceiverHw<FamilyType>::create("aubfile", true, executionEnvironment, 0, 1)));

    auto tbxCsrWithAubDump2 = std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>>(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(
        TbxCommandStreamReceiverHw<FamilyType>::create("aubfile", true, executionEnvironment, 0, 1)));

    auto mockManager = reinterpret_cast<MockAubManager *>(executionEnvironment.rootDeviceEnvironments[0]->aubCenter->getAubManager());
    EXPECT_EQ(1u, mockManager->openCalledCnt);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrInSubCaptureModeWhenFlushIsCalledAndSubCaptureIsDisabledThenPauseShouldBeTurnedOn) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment, pDevice->getDeviceBitfield()};
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    tbxCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    EXPECT_FALSE(tbxCsr.subCaptureManager->isSubCaptureEnabled());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    ResidencyContainer allocationsForResidency = {};

    tbxCsr.flush(batchBuffer, allocationsForResidency);

    auto mockAubManager = reinterpret_cast<MockAubManager *>(pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter->getAubManager());
    EXPECT_TRUE(mockAubManager->isPaused);

    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrInSubCaptureModeWhenFlushIsCalledAndSubCaptureIsEnabledThenPauseShouldBeTurnedOff) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment, pDevice->getDeviceBitfield()};
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureManagerMock->setSubCaptureIsActive(true);
    tbxCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    EXPECT_TRUE(tbxCsr.subCaptureManager->isSubCaptureEnabled());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    ResidencyContainer allocationsForResidency = {};

    tbxCsr.flush(batchBuffer, allocationsForResidency);

    auto mockAubManager = reinterpret_cast<MockAubManager *>(pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter->getAubManager());
    EXPECT_FALSE(mockAubManager->isPaused);

    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrInSubCaptureModeWhenFlushIsCalledAndSubCaptureIsEnabledThenCallPollForCompletionAndDisableSubCapture) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment, pDevice->getDeviceBitfield()};
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureManagerMock->setSubCaptureIsActive(true);
    tbxCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    EXPECT_TRUE(tbxCsr.subCaptureManager->isSubCaptureEnabled());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    ResidencyContainer allocationsForResidency = {};

    tbxCsr.flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(tbxCsr.pollForCompletionCalled);
    EXPECT_FALSE(tbxCsr.subCaptureManager->isSubCaptureEnabled());

    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrInSubCaptureModeWhenFlushIsCalledAndSubCaptureGetsActivatedThenCallSubmitBatchBufferWithOverrideRingBufferSetToTrue) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment, pDevice->getDeviceBitfield()};
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureManagerMock->setSubCaptureIsActive(true);
    tbxCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    EXPECT_FALSE(aubSubCaptureManagerMock->wasSubCaptureActiveInPreviousEnqueue());
    EXPECT_TRUE(aubSubCaptureManagerMock->isSubCaptureActive());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    ResidencyContainer allocationsForResidency = {};

    tbxCsr.flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(tbxCsr.submitBatchBufferCalled);
    EXPECT_TRUE(tbxCsr.overrideRingHeadPassed);

    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrInSubCaptureModeWhenFlushIsCalledAndSubCaptureRemainsActiveThenCallSubmitBatchBufferWithOverrideRingBufferSetToTrue) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment, pDevice->getDeviceBitfield()};
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureManagerMock->setSubCaptureWasActiveInPreviousEnqueue(true);
    aubSubCaptureManagerMock->setSubCaptureIsActive(true);
    tbxCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    EXPECT_TRUE(aubSubCaptureManagerMock->wasSubCaptureActiveInPreviousEnqueue());
    EXPECT_TRUE(aubSubCaptureManagerMock->isSubCaptureActive());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
    ResidencyContainer allocationsForResidency = {};

    tbxCsr.flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(tbxCsr.submitBatchBufferCalled);
    EXPECT_FALSE(tbxCsr.overrideRingHeadPassed);

    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenProcessResidencyIsCalledWithDumpTbxNonWritableFlagThenAllocationsForResidencyShouldBeMadeTbxWritable) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockTbxCsrToTestDumpTbxNonWritable<FamilyType>> tbxCsr(new MockTbxCsrToTestDumpTbxNonWritable<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    memoryManager.reset(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));
    tbxCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::buffer, pDevice->getDeviceBitfield()});
    tbxCsr->setTbxWritable(false, *gfxAllocation);

    tbxCsr->dumpTbxNonWritable = true;

    ResidencyContainer allocationsForResidency = {gfxAllocation};
    tbxCsr->processResidency(allocationsForResidency, 0u);

    EXPECT_TRUE(tbxCsr->isTbxWritable(*gfxAllocation));
    EXPECT_FALSE(tbxCsr->dumpTbxNonWritable);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenProcessResidencyIsCalledWithoutDumpTbxWritableFlagThenAllocationsForResidencyShouldBeKeptNonTbxWritable) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockTbxCsrToTestDumpTbxNonWritable<FamilyType>> tbxCsr(new MockTbxCsrToTestDumpTbxNonWritable<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    memoryManager.reset(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));
    tbxCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::buffer, pDevice->getDeviceBitfield()});
    tbxCsr->setTbxWritable(false, *gfxAllocation);

    EXPECT_FALSE(tbxCsr->dumpTbxNonWritable);

    ResidencyContainer allocationsForResidency = {gfxAllocation};
    tbxCsr->processResidency(allocationsForResidency, 0u);

    EXPECT_FALSE(tbxCsr->isTbxWritable(*gfxAllocation));
    EXPECT_FALSE(tbxCsr->dumpTbxNonWritable);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrInSubCaptureModeWhenCheckAndActivateAubSubCaptureIsCalledAndSubCaptureIsInactiveThenDontForceDumpingAllocationsTbxNonWritable) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment, pDevice->getDeviceBitfield()};
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    tbxCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);

    EXPECT_FALSE(tbxCsr.dumpTbxNonWritable);

    auto status = tbxCsr.checkAndActivateAubSubCapture("");
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);

    EXPECT_FALSE(tbxCsr.dumpTbxNonWritable);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrInSubCaptureModeWhenCheckAndActivateAubSubCaptureIsCalledAndSubCaptureGetsActivatedThenForceDumpingAllocationsTbxNonWritable) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment, pDevice->getDeviceBitfield()};
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    aubSubCaptureManagerMock->setSubCaptureIsActive(false);
    aubSubCaptureManagerMock->setSubCaptureToggleActive(true);
    tbxCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);

    EXPECT_FALSE(tbxCsr.dumpTbxNonWritable);

    auto status = tbxCsr.checkAndActivateAubSubCapture("kernelName");
    EXPECT_TRUE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);

    EXPECT_TRUE(tbxCsr.dumpTbxNonWritable);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrInSubCaptureModeWhenCheckAndActivateAubSubCaptureIsCalledAndSubCaptureRemainsActivatedThenDontForceDumpingAllocationsTbxNonWritable) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment, pDevice->getDeviceBitfield()};
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::toggle;
    aubSubCaptureManagerMock->setSubCaptureIsActive(true);
    aubSubCaptureManagerMock->setSubCaptureToggleActive(true);
    tbxCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);

    EXPECT_FALSE(tbxCsr.dumpTbxNonWritable);

    auto status = tbxCsr.checkAndActivateAubSubCapture("kernelName");
    EXPECT_TRUE(status.isActive);
    EXPECT_TRUE(status.wasActiveInPreviousEnqueue);

    EXPECT_FALSE(tbxCsr.dumpTbxNonWritable);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrInNonSubCaptureModeWhenCheckAndActivateAubSubCaptureIsCalledThenReturnStatusInactive) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment, pDevice->getDeviceBitfield()};
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    auto status = tbxCsr.checkAndActivateAubSubCapture("");
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);
}

HWTEST_F(TbxCommandStreamTests, givenGraphicsAllocationWhenDumpAllocationIsCalledButBcsIsUsedThenGraphicsAllocationShouldNotBeDumped) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::buffer, pDevice->getDeviceBitfield()});

    tbxCsr.dumpAllocation(*gfxAllocation);
    EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenGraphicsAllocationWritableWhenDumpAllocationIsCalledButDumpFormatIsNotSpecifiedThenGraphicsAllocationShouldNotBeDumped) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::buffer, pDevice->getDeviceBitfield()});

    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    tbxCsr.dumpAllocation(*gfxAllocation);
    EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenGraphicsAllocationWritableWhenDumpAllocationIsCalledAndDumpFormatIsSpecifiedThenGraphicsAllocationShouldBeDumped) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::buffer, pDevice->getDeviceBitfield()});

    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    gfxAllocation->setDefaultGmm(new Gmm(pDevice->executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));

    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    tbxCsr.dumpAllocation(*gfxAllocation);
    EXPECT_TRUE(mockHardwareContext->dumpSurfaceCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenGraphicsAllocationWhenDumpAllocationIsCalledAndAUBDumpAllocsOnEnqueueReadOnlyIsSetThenDumpableFlagShouldBeRespected) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.AUBDumpAllocsOnEnqueueReadOnly.set(true);
    debugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::buffer, pDevice->getDeviceBitfield()});

    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    gfxAllocation->setAllocDumpable(false, false);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    gfxAllocation->setDefaultGmm(new Gmm(pDevice->executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));

    tbxCsr.dumpAllocation(*gfxAllocation);
    EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);
    EXPECT_FALSE(gfxAllocation->isAllocDumpable());

    auto &csrOsContext = static_cast<MockOsContext &>(tbxCsr.getOsContext());
    {
        // Non-BCS engine, BCS dump
        EXPECT_FALSE(EngineHelpers::isBcs(csrOsContext.getEngineType()));
        gfxAllocation->setAllocDumpable(true, true);

        tbxCsr.dumpAllocation(*gfxAllocation);

        EXPECT_TRUE(gfxAllocation->isAllocDumpable());
        EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);
    }

    {
        // Non-BCS engine, Non-BCS dump
        EXPECT_FALSE(EngineHelpers::isBcs(csrOsContext.getEngineType()));
        gfxAllocation->setAllocDumpable(true, false);

        tbxCsr.dumpAllocation(*gfxAllocation);

        EXPECT_FALSE(gfxAllocation->isAllocDumpable());
        EXPECT_TRUE(mockHardwareContext->dumpSurfaceCalled);
        mockHardwareContext->dumpSurfaceCalled = false;
    }

    {
        // BCS engine, Non-BCS dump
        csrOsContext.engineType = aub_stream::EngineType::ENGINE_BCS;
        EXPECT_TRUE(EngineHelpers::isBcs(csrOsContext.getEngineType()));
        gfxAllocation->setAllocDumpable(true, false);

        tbxCsr.dumpAllocation(*gfxAllocation);

        EXPECT_TRUE(gfxAllocation->isAllocDumpable());
        EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);
    }

    {
        // BCS engine, BCS dump
        csrOsContext.engineType = aub_stream::EngineType::ENGINE_BCS;
        EXPECT_TRUE(EngineHelpers::isBcs(csrOsContext.getEngineType()));
        gfxAllocation->setAllocDumpable(true, true);

        tbxCsr.dumpAllocation(*gfxAllocation);

        EXPECT_FALSE(gfxAllocation->isAllocDumpable());
        EXPECT_TRUE(mockHardwareContext->dumpSurfaceCalled);
    }

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenGraphicsAllocationWhenDumpAllocationIsCalledAndAUBDumpAllocsOnEnqueueSVMMemcpyOnlyIsSetThenDumpableFlagShouldBeRespected) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.AUBDumpAllocsOnEnqueueSVMMemcpyOnly.set(true);
    debugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::buffer, pDevice->getDeviceBitfield()});

    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    gfxAllocation->setAllocDumpable(false, false);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    gfxAllocation->setDefaultGmm(new Gmm(pDevice->executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));

    tbxCsr.dumpAllocation(*gfxAllocation);
    EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);
    EXPECT_FALSE(gfxAllocation->isAllocDumpable());

    gfxAllocation->setAllocDumpable(true, false);
    tbxCsr.dumpAllocation(*gfxAllocation);
    EXPECT_FALSE(gfxAllocation->isAllocDumpable());
    EXPECT_TRUE(mockHardwareContext->dumpSurfaceCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenGraphicsAllocationWhenDumpAllocationIsCalledButUseAubStreamIsSetToFalseThenEarlyReturn) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.UseAubStream.set(false);
    debugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), false, 1);
    executionEnvironment.initializeMemoryManager();
    executionEnvironment.initGmm();

    MockTbxCsr<FamilyType> tbxCsr(executionEnvironment, 1);
    EXPECT_EQ(nullptr, executionEnvironment.rootDeviceEnvironments[0]->aubCenter->getAubManager());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::buffer, pDevice->getDeviceBitfield()});

    tbxCsr.dumpAllocation(*gfxAllocation);
    EXPECT_TRUE(tbxCsr.dumpAllocationCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTimestampBufferAllocationWhenTbxWriteMemoryIsCalledForAllocationThenItIsOneTimeWriteable) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    tbxCsr->initializeEngine();
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    size_t alignedSize = MemoryConstants::pageSize64k;
    AllocationType allocationType = NEO::AllocationType::gpuTimestampDeviceBuffer;

    AllocationProperties allocationProperties{pDevice->getRootDeviceIndex(),
                                              true,
                                              alignedSize,
                                              allocationType,
                                              false,
                                              false,
                                              pDevice->getDeviceBitfield()};

    auto timestampAllocation = memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties);
    ASSERT_NE(nullptr, timestampAllocation);

    timestampAllocation->setTbxWritable(true, GraphicsAllocation::defaultBank);

    EXPECT_TRUE(tbxCsr->writeMemory(*timestampAllocation));
    EXPECT_FALSE(timestampAllocation->isTbxWritable(GraphicsAllocation::defaultBank));

    memoryManager->freeGraphicsMemory(timestampAllocation);
}

template <typename FamilyType>
class MockTbxCsrForPageFaultTests : public MockTbxCsr<FamilyType> {
  public:
    using MockTbxCsr<FamilyType>::MockTbxCsr;

    CpuPageFaultManager *getTbxPageFaultManager() override {
        return this->tbxFaultManager.get();
    }

    using MockTbxCsr<FamilyType>::isAllocTbxFaultable;

    std::unique_ptr<TbxPageFaultManager> tbxFaultManager = TbxPageFaultManager::create();
};

HWTEST_F(TbxCommandStreamTests, givenTbxModeWhenHostWritesHostAllocThenAllocShouldBeDownloadedAndWritable) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::tbx));
    debugManager.flags.EnableTbxPageFaultManager.set(1);
    std::unique_ptr<MockTbxCsrForPageFaultTests<FamilyType>> tbxCsr(new MockTbxCsrForPageFaultTests<FamilyType>(*pDevice->executionEnvironment, pDevice->getDeviceBitfield()));
    tbxCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    EXPECT_TRUE(tbxCsr->tbxFaultManager->checkFaultHandlerFromPageFaultManager());

    auto memoryManager = pDevice->getMemoryManager();

    NEO::GraphicsAllocation *gfxAlloc1 = memoryManager->allocateGraphicsMemoryWithProperties(
        {pDevice->getRootDeviceIndex(),
         MemoryConstants::pageSize,
         AllocationType::bufferHostMemory,
         pDevice->getDeviceBitfield()});

    uint64_t gpuAddress;
    void *cpuAddress;
    size_t size;

    EXPECT_TRUE(tbxCsr->getParametersForMemory(*gfxAlloc1, gpuAddress, cpuAddress, size));

    tbxCsr->writeMemory(*gfxAlloc1);
    EXPECT_FALSE(tbxCsr->isTbxWritable(*gfxAlloc1));

    // accessing outside address range does not affect inserted host allocs
    auto ptrBelow = (void *)0x0;
    EXPECT_FALSE(tbxCsr->tbxFaultManager->verifyAndHandlePageFault(ptrBelow, true));
    auto ptrAbove = ptrOffset(cpuAddress, size + 1);
    EXPECT_FALSE(tbxCsr->tbxFaultManager->verifyAndHandlePageFault(ptrAbove, true));
    EXPECT_FALSE(tbxCsr->isTbxWritable(*gfxAlloc1));

    *reinterpret_cast<char *>(cpuAddress) = 1;
    EXPECT_TRUE(tbxCsr->isTbxWritable(*gfxAlloc1));
    EXPECT_TRUE(tbxCsr->makeCoherentCalled);
    tbxCsr->makeCoherentCalled = false;

    tbxCsr->writeMemory(*gfxAlloc1);
    EXPECT_FALSE(tbxCsr->isTbxWritable(*gfxAlloc1));

    // accessing address with offset that is still in alloc range should
    // also make writable and download
    reinterpret_cast<char *>(cpuAddress)[1] = 1;
    EXPECT_TRUE(tbxCsr->isTbxWritable(*gfxAlloc1));
    EXPECT_TRUE(tbxCsr->makeCoherentCalled);
    tbxCsr->makeCoherentCalled = false;

    // for coverage
    tbxCsr->tbxFaultManager->removeAllocation(static_cast<GraphicsAllocation *>(nullptr));
    tbxCsr->tbxFaultManager->removeAllocation(gfxAlloc1);

    memoryManager->freeGraphicsMemory(gfxAlloc1);
}

HWTEST_F(TbxCommandStreamTests, givenTbxWithModeWhenHostBufferNotWritableAndProtectedThenDownloadShouldNotCrash) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::tbx));
    debugManager.flags.EnableTbxPageFaultManager.set(1);
    std::unique_ptr<MockTbxCsrForPageFaultTests<FamilyType>> tbxCsr(new MockTbxCsrForPageFaultTests<FamilyType>(*pDevice->executionEnvironment, pDevice->getDeviceBitfield()));
    tbxCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    EXPECT_TRUE(tbxCsr->tbxFaultManager->checkFaultHandlerFromPageFaultManager());

    auto memoryManager = pDevice->getMemoryManager();

    NEO::GraphicsAllocation *gfxAlloc1 = memoryManager->allocateGraphicsMemoryWithProperties(
        {pDevice->getRootDeviceIndex(),
         MemoryConstants::pageSize,
         AllocationType::bufferHostMemory,
         pDevice->getDeviceBitfield()});

    uint64_t gpuAddress;
    void *cpuAddress;
    size_t size;

    EXPECT_TRUE(tbxCsr->getParametersForMemory(*gfxAlloc1, gpuAddress, cpuAddress, size));

    tbxCsr->writeMemory(*gfxAlloc1);
    tbxCsr->downloadAllocationTbx(*gfxAlloc1);
    EXPECT_TRUE(!tbxCsr->isTbxWritable(*gfxAlloc1));

    static_cast<float *>(cpuAddress)[0] = 1.0f;

    memoryManager->freeGraphicsMemory(gfxAlloc1);
}

HWTEST_F(TbxCommandStreamTests, givenAllocationWithNoDriverAllocatedCpuPtrThenIsAllocTbxFaultableShouldReturnFalse) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::tbx));
    debugManager.flags.EnableTbxPageFaultManager.set(1);
    std::unique_ptr<MockTbxCsrForPageFaultTests<FamilyType>> tbxCsr(new MockTbxCsrForPageFaultTests<FamilyType>(*pDevice->executionEnvironment, pDevice->getDeviceBitfield()));
    tbxCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    EXPECT_TRUE(tbxCsr->tbxFaultManager->checkFaultHandlerFromPageFaultManager());

    auto memoryManager = pDevice->getMemoryManager();

    NEO::GraphicsAllocation *gfxAlloc1 = memoryManager->allocateGraphicsMemoryWithProperties(
        {pDevice->getRootDeviceIndex(),
         MemoryConstants::pageSize,
         AllocationType::bufferHostMemory,
         pDevice->getDeviceBitfield()});

    auto cpuPtr = gfxAlloc1->getDriverAllocatedCpuPtr();

    gfxAlloc1->setDriverAllocatedCpuPtr(nullptr);
    EXPECT_FALSE(tbxCsr->isAllocTbxFaultable(gfxAlloc1));

    gfxAlloc1->setDriverAllocatedCpuPtr(cpuPtr);

    memoryManager->freeGraphicsMemory(gfxAlloc1);
}

HWTEST_F(TbxCommandStreamTests, givenTbxModeWhenHostReadsHostAllocThenAllocShouldBeDownloadedButNotWritable) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::tbx));
    debugManager.flags.EnableTbxPageFaultManager.set(1);
    std::unique_ptr<MockTbxCsrForPageFaultTests<FamilyType>> tbxCsr(new MockTbxCsrForPageFaultTests<FamilyType>(*pDevice->executionEnvironment, pDevice->getDeviceBitfield()));
    tbxCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    EXPECT_TRUE(tbxCsr->tbxFaultManager->checkFaultHandlerFromPageFaultManager());

    auto memoryManager = pDevice->getMemoryManager();

    NEO::GraphicsAllocation *gfxAlloc1 = memoryManager->allocateGraphicsMemoryWithProperties(
        {pDevice->getRootDeviceIndex(),
         MemoryConstants::pageSize,
         AllocationType::bufferHostMemory,
         pDevice->getDeviceBitfield()});

    uint64_t gpuAddress;
    void *cpuAddress;
    size_t size;

    EXPECT_TRUE(tbxCsr->getParametersForMemory(*gfxAlloc1, gpuAddress, cpuAddress, size));
    *reinterpret_cast<char *>(cpuAddress) = 1;

    tbxCsr->writeMemory(*gfxAlloc1);
    EXPECT_FALSE(tbxCsr->isTbxWritable(*gfxAlloc1));

    auto readVal = *reinterpret_cast<char *>(cpuAddress);
    EXPECT_EQ(1, readVal);
    EXPECT_FALSE(tbxCsr->isTbxWritable(*gfxAlloc1));
    EXPECT_TRUE(tbxCsr->makeCoherentCalled);
    tbxCsr->makeCoherentCalled = false;

    tbxCsr->writeMemory(*gfxAlloc1);
    EXPECT_FALSE(tbxCsr->isTbxWritable(*gfxAlloc1));

    readVal = *reinterpret_cast<char *>(cpuAddress);
    EXPECT_EQ(1, readVal);
    EXPECT_FALSE(tbxCsr->isTbxWritable(*gfxAlloc1));
    EXPECT_TRUE(tbxCsr->makeCoherentCalled);
    tbxCsr->makeCoherentCalled = false;

    // for coverage
    tbxCsr->tbxFaultManager->removeAllocation(static_cast<GraphicsAllocation *>(nullptr));
    tbxCsr->tbxFaultManager->removeAllocation(gfxAlloc1);

    memoryManager->freeGraphicsMemory(gfxAlloc1);
}

HWTEST_F(TbxCommandStreamTests, givenTbxModeWhenHandleFaultFalseThenTbxFaultableTypesShouldNotBeHandled) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::tbx));
    debugManager.flags.EnableTbxPageFaultManager.set(1);
    std::unique_ptr<MockTbxCsrForPageFaultTests<FamilyType>> tbxCsr(new MockTbxCsrForPageFaultTests<FamilyType>(*pDevice->executionEnvironment, pDevice->getDeviceBitfield()));
    tbxCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    EXPECT_TRUE(tbxCsr->tbxFaultManager->checkFaultHandlerFromPageFaultManager());

    auto memoryManager = pDevice->getMemoryManager();

    NEO::GraphicsAllocation *gfxAlloc1 = memoryManager->allocateGraphicsMemoryWithProperties(
        {pDevice->getRootDeviceIndex(),
         MemoryConstants::pageSize,
         AllocationType::bufferHostMemory,
         pDevice->getDeviceBitfield()});

    uint64_t gpuAddress;
    void *cpuAddress;
    size_t size;

    EXPECT_TRUE(tbxCsr->getParametersForMemory(*gfxAlloc1, gpuAddress, cpuAddress, size));
    *reinterpret_cast<char *>(cpuAddress) = 1;

    tbxCsr->writeMemory(*gfxAlloc1);
    EXPECT_FALSE(tbxCsr->isTbxWritable(*gfxAlloc1));

    auto readVal = *reinterpret_cast<char *>(cpuAddress);
    EXPECT_EQ(1, readVal);
    EXPECT_FALSE(tbxCsr->isTbxWritable(*gfxAlloc1));
    EXPECT_TRUE(tbxCsr->makeCoherentCalled);
    tbxCsr->makeCoherentCalled = false;

    tbxCsr->writeMemory(*gfxAlloc1);
    EXPECT_FALSE(tbxCsr->isTbxWritable(*gfxAlloc1));

    EXPECT_TRUE(tbxCsr->tbxFaultManager->verifyAndHandlePageFault(cpuAddress, false));
    EXPECT_FALSE(tbxCsr->makeCoherentCalled);

    tbxCsr->tbxFaultManager->removeAllocation(gfxAlloc1);

    memoryManager->freeGraphicsMemory(gfxAlloc1);
}

HWTEST_F(TbxCommandStreamTests, givenTbxModeWhenPageFaultManagerIsDisabledThenIsAllocTbxFaultableShouldReturnFalse) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::tbx));
    debugManager.flags.EnableTbxPageFaultManager.set(0);
    std::unique_ptr<MockTbxCsrForPageFaultTests<FamilyType>> tbxCsr(new MockTbxCsrForPageFaultTests<FamilyType>(*pDevice->executionEnvironment, pDevice->getDeviceBitfield()));
    tbxCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    EXPECT_TRUE(tbxCsr->tbxFaultManager->checkFaultHandlerFromPageFaultManager());

    auto memoryManager = pDevice->getMemoryManager();

    NEO::GraphicsAllocation *gfxAlloc1 = memoryManager->allocateGraphicsMemoryWithProperties(
        {pDevice->getRootDeviceIndex(),
         MemoryConstants::pageSize,
         AllocationType::bufferHostMemory,
         pDevice->getDeviceBitfield()});
    EXPECT_FALSE(tbxCsr->isAllocTbxFaultable(gfxAlloc1));

    memoryManager->freeGraphicsMemory(gfxAlloc1);
}

HWTEST_F(TbxCommandStreamTests, givenTbxModeWhenPageFaultManagerIsNotAvailableThenIsAllocTbxFaultableShouldReturnFalse) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::tbx));
    debugManager.flags.EnableTbxPageFaultManager.set(0);
    std::unique_ptr<MockTbxCsrForPageFaultTests<FamilyType>> tbxCsr(new MockTbxCsrForPageFaultTests<FamilyType>(*pDevice->executionEnvironment, pDevice->getDeviceBitfield()));
    tbxCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    tbxCsr->tbxFaultManager.reset(nullptr);

    auto memoryManager = pDevice->getMemoryManager();

    NEO::GraphicsAllocation *gfxAlloc1 = memoryManager->allocateGraphicsMemoryWithProperties(
        {pDevice->getRootDeviceIndex(),
         MemoryConstants::pageSize,
         AllocationType::bufferHostMemory,
         pDevice->getDeviceBitfield()});

    EXPECT_FALSE(tbxCsr->isAllocTbxFaultable(gfxAlloc1));

    memoryManager->freeGraphicsMemory(gfxAlloc1);
}

static constexpr std::array onceWritableAllocTypesForTbx{
    AllocationType::pipe,
    AllocationType::constantSurface,
    AllocationType::globalSurface,
    AllocationType::kernelIsa,
    AllocationType::kernelIsaInternal,
    AllocationType::privateSurface,
    AllocationType::scratchSurface,
    AllocationType::workPartitionSurface,
    AllocationType::buffer,
    AllocationType::image,
    AllocationType::timestampPacketTagBuffer,
    AllocationType::externalHostPtr,
    AllocationType::mapAllocation,
    AllocationType::svmGpu,
    AllocationType::gpuTimestampDeviceBuffer,
    AllocationType::assertBuffer,
    AllocationType::tagBuffer,
    AllocationType::syncDispatchToken,
    AllocationType::bufferHostMemory,
};

HWTEST_F(TbxCommandStreamTests, givenAubOneTimeWritableAllocWhenTbxFaultManagerIsAvailableAndAllocIsTbxFaultableThenTbxFaultableTypesShouldReturnTrue) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::tbx));
    debugManager.flags.EnableTbxPageFaultManager.set(1);
    std::unique_ptr<MockTbxCsrForPageFaultTests<FamilyType>> tbxCsr(new MockTbxCsrForPageFaultTests<FamilyType>(*pDevice->executionEnvironment, pDevice->getDeviceBitfield()));
    tbxCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    auto memoryManager = pDevice->getMemoryManager();

    NEO::GraphicsAllocation *gfxAlloc1 = memoryManager->allocateGraphicsMemoryWithProperties(
        {pDevice->getRootDeviceIndex(),
         MemoryConstants::pageSize,
         AllocationType::bufferHostMemory,
         pDevice->getDeviceBitfield()});

    auto backupAllocType = gfxAlloc1->getAllocationType();

    for (const auto &allocType : onceWritableAllocTypesForTbx) {
        gfxAlloc1->setAllocationType(allocType);
        if (allocType == AllocationType::bufferHostMemory) {
            EXPECT_TRUE(tbxCsr->isAllocTbxFaultable(gfxAlloc1));
        }
    }

    gfxAlloc1->setAllocationType(backupAllocType);

    memoryManager->freeGraphicsMemory(gfxAlloc1);
}

HWTEST_F(TbxCommandStreamTests, givenAubOneTimeWritableAllocWhenTbxFaultManagerIsAvailableAndAllocIsNotTbxFaultableThenTbxFaultableTypesShouldReturnFalse) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::tbx));
    debugManager.flags.EnableTbxPageFaultManager.set(1);
    std::unique_ptr<MockTbxCsrForPageFaultTests<FamilyType>> tbxCsr(new MockTbxCsrForPageFaultTests<FamilyType>(*pDevice->executionEnvironment, pDevice->getDeviceBitfield()));
    tbxCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    auto memoryManager = pDevice->getMemoryManager();

    NEO::GraphicsAllocation *gfxAlloc1 = memoryManager->allocateGraphicsMemoryWithProperties(
        {pDevice->getRootDeviceIndex(),
         MemoryConstants::pageSize,
         AllocationType::bufferHostMemory,
         pDevice->getDeviceBitfield()});

    auto backupAllocType = gfxAlloc1->getAllocationType();

    for (const auto &allocType : onceWritableAllocTypesForTbx) {
        gfxAlloc1->setAllocationType(allocType);
        if (allocType != AllocationType::bufferHostMemory && allocType != AllocationType::timestampPacketTagBuffer) {
            EXPECT_FALSE(tbxCsr->isAllocTbxFaultable(gfxAlloc1));
        }
    }

    gfxAlloc1->setAllocationType(backupAllocType);

    memoryManager->freeGraphicsMemory(gfxAlloc1);
}

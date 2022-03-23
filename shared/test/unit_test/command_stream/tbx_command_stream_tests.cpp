/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_alloc_dump.h"
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/hardware_context_controller.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/fixtures/tbx_command_stream_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_aub_subcapture_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_tbx_csr.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/mock_aub_center_fixture.h"

#include <cstdint>

using namespace NEO;

namespace NEO {
extern TbxCommandStreamReceiverCreateFunc tbxCommandStreamReceiverFactory[IGFX_MAX_CORE];
} // namespace NEO

struct TbxFixture : public TbxCommandStreamFixture,
                    public DeviceFixture,
                    public MockAubCenterFixture {

    using TbxCommandStreamFixture::SetUp;

    TbxFixture() : MockAubCenterFixture(CommandStreamReceiverType::CSR_TBX) {}

    void SetUp() {
        DeviceFixture::SetUp();
        setMockAubCenter(*pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]);
        TbxCommandStreamFixture::SetUp(pDevice);
        MockAubCenterFixture::SetUp();
    }

    void TearDown() {
        MockAubCenterFixture::TearDown();
        TbxCommandStreamFixture::TearDown();
        DeviceFixture::TearDown();
    }
};

using TbxCommandStreamTests = Test<TbxFixture>;
using TbxCommandSteamSimpleTest = TbxCommandStreamTests;

template <typename GfxFamily>
struct MockTbxCsrToTestDumpTbxNonWritable : public TbxCommandStreamReceiverHw<GfxFamily> {
    using TbxCommandStreamReceiverHw<GfxFamily>::TbxCommandStreamReceiverHw;
    using TbxCommandStreamReceiverHw<GfxFamily>::dumpTbxNonWritable;

    bool writeMemory(GraphicsAllocation &gfxAllocation) override {
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
    EXPECT_EQ(CommandStreamReceiverType::CSR_TBX, csr->getType());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenMakeResidentIsCalledForGraphicsAllocationThenItShouldPushAllocationForResidencyToCsr) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pCommandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_EQ(0u, tbxCsr->getResidencyAllocations().size());

    tbxCsr->makeResident(*graphicsAllocation);

    EXPECT_EQ(1u, tbxCsr->getResidencyAllocations().size());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenItIsCreatedWithAubDumpAndAubCaptureFileNameHasBeenSpecifiedThenItShouldBeUsedToOpenTheFileWithAubCapture) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AUBDumpCaptureFileName.set("aubcapture_file_name.aub");

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

    EXPECT_EQ(0u, tbxCsr->getResidencyAllocations().size());

    tbxCsr->makeResident(*graphicsAllocation);

    EXPECT_EQ(1u, tbxCsr->getResidencyAllocations().size());

    tbxCsr->makeResident(*graphicsAllocation);

    EXPECT_EQ(1u, tbxCsr->getResidencyAllocations().size());

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

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pCommandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});
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

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pCommandStreamReceiver->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

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
    DebugManager.flags.OverrideAubDeviceId.set(9); //this is Hsw, not used
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

    tbxCsr.makeSurfacePackNonResident(allocationsForResidency);
    std::set<GraphicsAllocation *> expectedAllocationsForDownload = {&allocation1, &allocation3};
    EXPECT_EQ(expectedAllocationsForDownload, tbxCsr.allocationsForDownload);
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

    tbxCsr.waitForCompletionWithTimeout(WaitParams{false, true, 0}, 0);

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
    tbxCsr.flushSubmissionsAndDownloadAllocations(1u);
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

    EXPECT_EQ(0u, tbxCsr.obtainUniqueOwnershipCalled);
    tbxCsr.downloadAllocations();
    EXPECT_EQ(1u, tbxCsr.obtainUniqueOwnershipCalled);

    std::set<GraphicsAllocation *> expectedDownloadedAllocations = {tbxCsr.getTagAllocation(), &allocation1, &allocation2, &allocation3};
    EXPECT_EQ(0u, tbxCsr.allocationsForDownload.size());
}

HWTEST_F(TbxCommandSteamSimpleTest, whenTbxCommandStreamReceiverIsCreatedThenPPGTTAndGGTTCreatedHavePhysicalAddressAllocatorSet) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());

    uintptr_t address = 0x20000;
    auto physicalAddress = tbxCsr.ppgtt->map(address, MemoryConstants::pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(0u, physicalAddress);

    physicalAddress = tbxCsr.ggtt->map(address, MemoryConstants::pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(0u, physicalAddress);
}

HWTEST_F(TbxCommandSteamSimpleTest, givenTbxCommandStreamReceiverWhenPhysicalAddressAllocatorIsCreatedThenItIsNotNull) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    std::unique_ptr<PhysicalAddressAllocator> allocator(tbxCsr.createPhysicalAddressAllocator(&hardwareInfo));
    ASSERT_NE(nullptr, allocator);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenItIsCreatedWithUseAubStreamFalseThenDontInitializeAubManager) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.UseAubStream.set(false);
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 1, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
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

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverInBatchedModeWhenFlushIsCalledThenItShouldMakeCommandBufferResident) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::BatchedDispatch));

    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, pDevice->getDeviceBitfield()});

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 1, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
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

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenHardwareContextIsCreatedThenTbxStreamInCsrIsNotInitialized) {
    auto gmmHelper = pDevice->executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(&pDevice->getHardwareInfo(), *gmmHelper, false, "", CommandStreamReceiverType::CSR_TBX);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);

    pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);
    auto tbxCsr = std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>>(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(
        TbxCommandStreamReceiverHw<FamilyType>::create("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield())));

    EXPECT_FALSE(tbxCsr->streamInitialized);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenOsContextIsSetThenCreateHardwareContext) {
    auto hwInfo = pDevice->getHardwareInfo();
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo)[0], pDevice->getDeviceBitfield()));
    std::string fileName = "";
    MockAubManager *mockManager = new MockAubManager();
    auto gmmHelper = pDevice->executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();
    MockAubCenter *mockAubCenter = new MockAubCenter(&hwInfo, *gmmHelper, false, fileName, CommandStreamReceiverType::CSR_TBX);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);

    pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsr(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(TbxCommandStreamReceiver::create(fileName, false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield())));
    EXPECT_EQ(nullptr, tbxCsr->hardwareContextController.get());

    tbxCsr->setupContext(osContext);
    EXPECT_NE(nullptr, tbxCsr->hardwareContextController.get());
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
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment.initializeMemoryManager();
    executionEnvironment.initGmm();

    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    setMockAubCenter(*rootDeviceEnvironment, CommandStreamReceiverType::CSR_TBX);

    auto fullName = AUBCommandStreamReceiver::createFullFilePath(*defaultHwInfo, "aubfile", mockRootDeviceIndex);

    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsr(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(TbxCommandStreamReceiver::create("aubfile", true, executionEnvironment, 0, 1)));
    EXPECT_STREQ(fullName.c_str(), rootDeviceEnvironment->aubFileNameReceived.c_str());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenCreatedWithAubDumpThenOpenIsCalledOnAubManagerToOpenFileStream) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment.initializeMemoryManager();

    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsrWithAubDump(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(
        TbxCommandStreamReceiver::create("aubfile", true, executionEnvironment, 0, 1)));
    EXPECT_TRUE(tbxCsrWithAubDump->aubManager->isOpen());
}

using SimulatedCsrTest = ::testing::Test;
HWTEST_F(SimulatedCsrTest, givenTbxCsrTypeWhenCreateCommandStreamReceiverThenProperAubCenterIsInitalized) {
    uint32_t expectedRootDeviceIndex = 10;
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.initializeMemoryManager();
    executionEnvironment.prepareRootDeviceEnvironments(expectedRootDeviceIndex + 2);

    auto rootDeviceEnvironment = new MockRootDeviceEnvironment(executionEnvironment);
    executionEnvironment.rootDeviceEnvironments[expectedRootDeviceIndex].reset(rootDeviceEnvironment);
    rootDeviceEnvironment->setHwInfo(defaultHwInfo.get());

    EXPECT_EQ(nullptr, executionEnvironment.rootDeviceEnvironments[expectedRootDeviceIndex]->aubCenter.get());
    EXPECT_FALSE(rootDeviceEnvironment->initAubCenterCalled);

    auto csr = std::make_unique<TbxCommandStreamReceiverHw<FamilyType>>(executionEnvironment, expectedRootDeviceIndex, 1);
    EXPECT_NE(nullptr, executionEnvironment.rootDeviceEnvironments[expectedRootDeviceIndex]->aubCenter.get());
    EXPECT_TRUE(rootDeviceEnvironment->initAubCenterCalled);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenCreatedWithAubDumpInSubCaptureModeThenCreateSubCaptureManagerAndGenerateSubCaptureFileName) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(static_cast<int32_t>(AubSubCaptureManager::SubCaptureMode::Filter));

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
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
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
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

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});
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

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});
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
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
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
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
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
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
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
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular}, pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});

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
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});

    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    tbxCsr.dumpAllocation(*gfxAllocation);
    EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenGraphicsAllocationWritableWhenDumpAllocationIsCalledAndDumpFormatIsSpecifiedThenGraphicsAllocationShouldBeDumped) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    tbxCsr.dumpAllocation(*gfxAllocation);
    EXPECT_TRUE(mockHardwareContext->dumpSurfaceCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenGraphicsAllocationWhenDumpAllocationIsCalledAndAUBDumpAllocsOnEnqueueReadOnlyIsSetThenDumpableFlagShouldBeRespected) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpAllocsOnEnqueueReadOnly.set(true);
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});

    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    gfxAllocation->setAllocDumpable(false, false);

    tbxCsr.dumpAllocation(*gfxAllocation);
    EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);
    EXPECT_FALSE(gfxAllocation->isAllocDumpable());

    auto &csrOsContext = tbxCsr.getOsContext();
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
        csrOsContext.getEngineType() = aub_stream::EngineType::ENGINE_BCS;
        EXPECT_TRUE(EngineHelpers::isBcs(csrOsContext.getEngineType()));
        gfxAllocation->setAllocDumpable(true, false);

        tbxCsr.dumpAllocation(*gfxAllocation);

        EXPECT_TRUE(gfxAllocation->isAllocDumpable());
        EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);
    }

    {
        // BCS engine, BCS dump
        csrOsContext.getEngineType() = aub_stream::EngineType::ENGINE_BCS;
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
    DebugManager.flags.AUBDumpAllocsOnEnqueueSVMMemcpyOnly.set(true);
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment, pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});

    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    gfxAllocation->setAllocDumpable(false, false);

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
    DebugManager.flags.UseAubStream.set(false);
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), false, 1);
    executionEnvironment.initializeMemoryManager();
    executionEnvironment.initGmm();

    MockTbxCsr<FamilyType> tbxCsr(executionEnvironment, 1);
    EXPECT_EQ(nullptr, executionEnvironment.rootDeviceEnvironments[0]->aubCenter->getAubManager());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});

    tbxCsr.dumpAllocation(*gfxAllocation);
    EXPECT_TRUE(tbxCsr.dumpAllocationCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

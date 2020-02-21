/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/command_stream/aub_command_stream_receiver.h"
#include "opencl/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "opencl/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "opencl/source/helpers/hardware_context_controller.h"
#include "opencl/source/mem_obj/mem_obj.h"
#include "opencl/source/memory_manager/memory_banks.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/fixtures/mock_aub_center_fixture.h"
#include "opencl/test/unit_test/gen_common/gen_cmd_parse.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/mocks/mock_aub_center.h"
#include "opencl/test/unit_test/mocks/mock_aub_manager.h"
#include "opencl/test/unit_test/mocks/mock_aub_subcapture_manager.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_os_context.h"
#include "opencl/test/unit_test/mocks/mock_tbx_csr.h"
#include "test.h"

#include "tbx_command_stream_fixture.h"

#include <cstdint>

using namespace NEO;

namespace NEO {
extern TbxCommandStreamReceiverCreateFunc tbxCommandStreamReceiverFactory[IGFX_MAX_CORE];
} // namespace NEO

namespace Os {
extern const char *tbxLibName;
}

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

    void TearDown() override {
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

TEST_F(TbxCommandStreamTests, DISABLED_makeResident) {
    uint8_t buffer[0x10000];
    size_t size = sizeof(buffer);

    GraphicsAllocation *graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, size}, buffer);
    pCommandStreamReceiver->makeResident(*graphicsAllocation);
    pCommandStreamReceiver->makeNonResident(*graphicsAllocation);
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(TbxCommandStreamTests, DISABLED_makeResidentOnZeroSizedBufferShouldDoNothing) {
    MockGraphicsAllocation graphicsAllocation(nullptr, 0);

    pCommandStreamReceiver->makeResident(graphicsAllocation);
    pCommandStreamReceiver->makeNonResident(graphicsAllocation);
}

TEST_F(TbxCommandStreamTests, DISABLED_flush) {
    char buffer[4096];
    memset(buffer, 0, 4096);
    LinearStream cs(buffer, 4096);
    size_t startOffset = 0;
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), startOffset, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    pCommandStreamReceiver->flush(batchBuffer, pCommandStreamReceiver->getResidencyAllocations());
}

HWTEST_F(TbxCommandStreamTests, DISABLED_flushUntilTailRingBufferLargerThanSizeRingBuffer) {
    char buffer[4096];
    memset(buffer, 0, 4096);
    LinearStream cs(buffer, 4096);
    size_t startOffset = 0;
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), startOffset, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    pCommandStreamReceiver->flush(batchBuffer, pCommandStreamReceiver->getResidencyAllocations());
    auto size = tbxCsr->engineInfo.sizeRingBuffer;
    tbxCsr->engineInfo.sizeRingBuffer = 64;

    pCommandStreamReceiver->flush(batchBuffer, pCommandStreamReceiver->getResidencyAllocations());
    pCommandStreamReceiver->flush(batchBuffer, pCommandStreamReceiver->getResidencyAllocations());
    pCommandStreamReceiver->flush(batchBuffer, pCommandStreamReceiver->getResidencyAllocations());
    tbxCsr->engineInfo.sizeRingBuffer = size;
}

HWTEST_F(TbxCommandStreamTests, DISABLED_getCsTraits) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    tbxCsr->getCsTraits(aub_stream::ENGINE_RCS);
    tbxCsr->getCsTraits(aub_stream::ENGINE_BCS);
    tbxCsr->getCsTraits(aub_stream::ENGINE_VCS);
    tbxCsr->getCsTraits(aub_stream::ENGINE_VECS);
}

TEST(TbxCommandStreamReceiverTest, givenNullFactoryEntryWhenTbxCsrIsCreatedThenNullptrIsReturned) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    GFXCORE_FAMILY family = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->platform.eRenderCoreFamily;
    VariableBackup<TbxCommandStreamReceiverCreateFunc> tbxCsrFactoryBackup(&tbxCommandStreamReceiverFactory[family]);

    tbxCommandStreamReceiverFactory[family] = nullptr;

    CommandStreamReceiver *csr = TbxCommandStreamReceiver::create("", false, *executionEnvironment, 0);
    EXPECT_EQ(nullptr, csr);
}

TEST(TbxCommandStreamReceiverTest, givenTbxCommandStreamReceiverWhenItIsCreatedWithWrongGfxCoreFamilyThenNullPointerShouldBeReturned) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    auto hwInfo = executionEnvironment->getMutableHardwareInfo();

    hwInfo->platform.eRenderCoreFamily = GFXCORE_FAMILY_FORCE_ULONG; // wrong gfx core family

    CommandStreamReceiver *csr = TbxCommandStreamReceiver::create("", false, *executionEnvironment, 0);
    EXPECT_EQ(nullptr, csr);
}

TEST(TbxCommandStreamReceiverTest, givenTbxCommandStreamReceiverWhenTypeIsCheckedThenTbxCsrIsReturned) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    std::unique_ptr<CommandStreamReceiver> csr(TbxCommandStreamReceiver::create("", false, *executionEnvironment, 0));
    EXPECT_NE(nullptr, csr);
    EXPECT_EQ(CommandStreamReceiverType::CSR_TBX, csr->getType());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenMakeResidentIsCalledForGraphicsAllocationThenItShouldPushAllocationForResidencyToCsr) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
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
        TbxCommandStreamReceiverHw<FamilyType>::create("aubfile", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex())));

    EXPECT_TRUE(tbxCsrWithAubDump->aubManager->isOpen());
    EXPECT_STREQ("aubcapture_file_name.aub", tbxCsrWithAubDump->aubManager->getFileName().c_str());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenMakeResidentHasAlreadyBeenCalledForGraphicsAllocationThenItShouldNotPushAllocationForResidencyAgainToCsr) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
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
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_TRUE(tbxCsr->writeMemory(*graphicsAllocation));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenWriteMemoryIsCalledWithGraphicsAllocationThatIsOnlyOneTimeWriteableThenGraphicsAllocationIsUpdated) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_TRUE(tbxCsr->isTbxWritable(*graphicsAllocation));
    EXPECT_TRUE(tbxCsr->writeMemory(*graphicsAllocation));
    EXPECT_FALSE(tbxCsr->isTbxWritable(*graphicsAllocation));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenWriteMemoryIsCalledWithGraphicsAllocationThatIsOnlyOneTimeWriteableButAlreadyWrittenThenGraphicsAllocationIsNotUpdated) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});
    ASSERT_NE(nullptr, graphicsAllocation);

    tbxCsr->setTbxWritable(false, *graphicsAllocation);
    EXPECT_FALSE(tbxCsr->writeMemory(*graphicsAllocation));
    EXPECT_FALSE(tbxCsr->isTbxWritable(*graphicsAllocation));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenWriteMemoryIsCalledForGraphicsAllocationWithZeroSizeThenItShouldReturnFalse) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    MockGraphicsAllocation graphicsAllocation((void *)0x1234, 0);

    EXPECT_FALSE(tbxCsr->writeMemory(graphicsAllocation));
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenProcessResidencyIsCalledWithoutAllocationsForResidencyThenItShouldProcessAllocationsFromMemoryManager) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
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
    MemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
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

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, graphicsAllocation);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};

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

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};

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
    const HardwareInfo &hwInfo = *platformDevices[0];
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(pCommandStreamReceiver);
    EXPECT_EQ(hwInfo.capabilityTable.aubDeviceId, tbxCsr->aubDeviceId);
}

HWTEST_F(TbxCommandStreamTests, givenDbgDeviceIdFlagIsSetWhenTbxCsrIsCreatedThenUseDebugDeviceId) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.OverrideAubDeviceId.set(9); //this is Hsw, not used
    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsr(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(TbxCommandStreamReceiver::create("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex())));
    EXPECT_EQ(9u, tbxCsr->aubDeviceId);
}

HWTEST_F(TbxCommandSteamSimpleTest, givenTbxCsrWhenCallingMakeSurfacePackNonResidentThenOnlyResidentAllocationsAddedAllocationsForDownload) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment};
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
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
    struct MockTbxCsr : TbxCommandStreamReceiverHw<FamilyType> {
        using CommandStreamReceiver::latestFlushedTaskCount;
        using TbxCommandStreamReceiverHw<FamilyType>::TbxCommandStreamReceiverHw;
        void downloadAllocation(GraphicsAllocation &gfxAllocation) override {
            *reinterpret_cast<uint32_t *>(CommandStreamReceiver::getTagAllocation()->getUnderlyingBuffer()) = this->latestFlushedTaskCount;
            downloadedAllocations.insert(&gfxAllocation);
        }
        std::set<GraphicsAllocation *> downloadedAllocations;
    };

    MockTbxCsr tbxCsr{*pDevice->executionEnvironment, pDevice->getRootDeviceIndex()};
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    uint32_t tag = 0u;
    tbxCsr.setupContext(osContext);
    tbxCsr.setTagAllocation(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, sizeof(tag)}, &tag));
    tbxCsr.latestFlushedTaskCount = 1u;

    MockGraphicsAllocation allocation1, allocation2, allocation3;
    allocation1.usageInfos[0].residencyTaskCount = 1;
    allocation2.usageInfos[0].residencyTaskCount = 1;
    allocation3.usageInfos[0].residencyTaskCount = 1;
    ASSERT_TRUE(allocation1.isResident(0u));
    ASSERT_TRUE(allocation2.isResident(0u));
    ASSERT_TRUE(allocation3.isResident(0u));

    tbxCsr.allocationsForDownload = {&allocation1, &allocation2, &allocation3};

    tbxCsr.waitForTaskCountWithKmdNotifyFallback(0u, 0u, false, false);

    std::set<GraphicsAllocation *> expectedDownloadedAllocations = {tbxCsr.getTagAllocation(), &allocation1, &allocation2, &allocation3};
    EXPECT_EQ(expectedDownloadedAllocations, tbxCsr.downloadedAllocations);
    EXPECT_EQ(0u, tbxCsr.allocationsForDownload.size());
}

HWTEST_F(TbxCommandSteamSimpleTest, whenTbxCommandStreamReceiverIsCreatedThenPPGTTAndGGTTCreatedHavePhysicalAddressAllocatorSet) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment);

    uintptr_t address = 0x20000;
    auto physicalAddress = tbxCsr.ppgtt->map(address, MemoryConstants::pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(0u, physicalAddress);

    physicalAddress = tbxCsr.ggtt->map(address, MemoryConstants::pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(0u, physicalAddress);
}

HWTEST_F(TbxCommandSteamSimpleTest, givenTbxCommandStreamReceiverWhenPhysicalAddressAllocatorIsCreatedThenItIsNotNull) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment);
    std::unique_ptr<PhysicalAddressAllocator> allocator(tbxCsr.createPhysicalAddressAllocator(&hardwareInfo));
    ASSERT_NE(nullptr, allocator);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenItIsCreatedWithUseAubStreamFalseThenDontInitializeAubManager) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.UseAubStream.set(false);
    MockExecutionEnvironment executionEnvironment(platformDevices[0], false, 1);
    executionEnvironment.initializeMemoryManager();

    auto tbxCsr = std::make_unique<TbxCommandStreamReceiverHw<FamilyType>>(executionEnvironment, 0);
    EXPECT_EQ(nullptr, executionEnvironment.rootDeviceEnvironments[0]->aubCenter->getAubManager());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenFlushIsCalledThenItShouldCallTheExpectedHwContextFunctions) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment);
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    tbxCsr.setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 1, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
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

    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment);
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    tbxCsr.setupContext(osContext);

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 1, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    ResidencyContainer allocationsForResidency;

    tbxCsr.flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(tbxCsr.writeMemoryWithAubManagerCalled);
    EXPECT_EQ(1u, batchBuffer.commandBufferAllocation->getResidencyTaskCount(tbxCsr.getOsContext().getContextId()));
    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenFlushIsCalledWithZeroSizedBufferThenSubmitIsNotCalledOnHwContext) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment);
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    tbxCsr.setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    ResidencyContainer allocationsForResidency;

    tbxCsr.flush(batchBuffer, allocationsForResidency);

    EXPECT_FALSE(mockHardwareContext->submitCalled);
    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenMakeResidentIsCalledThenItShouldCallTheExpectedHwContextFunctions) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment);
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    tbxCsr.setupContext(osContext);

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    ResidencyContainer allocationsForResidency = {&allocation};
    tbxCsr.processResidency(allocationsForResidency, 0u);

    EXPECT_TRUE(tbxCsr.writeMemoryWithAubManagerCalled);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenDownloadAllocationIsCalledThenItShouldCallTheExpectedHwContextFunctions) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment);
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    tbxCsr.setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    tbxCsr.downloadAllocation(allocation);

    EXPECT_TRUE(mockHardwareContext->readMemoryCalled);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenHardwareContextIsCreatedThenTbxStreamInCsrIsNotInitialized) {
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(pDevice->executionEnvironment->getHardwareInfo(), false, "", CommandStreamReceiverType::CSR_TBX);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);

    pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);
    auto tbxCsr = std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>>(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(
        TbxCommandStreamReceiverHw<FamilyType>::create("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex())));

    EXPECT_FALSE(tbxCsr->streamInitialized);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenOsContextIsSetThenCreateHardwareContext) {
    auto hwInfo = pDevice->getHardwareInfo();
    MockOsContext osContext(0, 1, HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo)[0],
                            PreemptionMode::Disabled, false);
    std::string fileName = "";
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(&hwInfo, false, fileName, CommandStreamReceiverType::CSR_TBX);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);

    pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsr(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(TbxCommandStreamReceiver::create(fileName, false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex())));
    EXPECT_EQ(nullptr, tbxCsr->hardwareContextController.get());

    tbxCsr->setupContext(osContext);
    EXPECT_NE(nullptr, tbxCsr->hardwareContextController.get());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenPollForCompletionImplIsCalledThenSimulatedCsrMethodIsCalled) {
    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsr(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(TbxCommandStreamReceiver::create("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex())));
    tbxCsr->pollForCompletionImpl();
}
HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenItIsQueriedForPreferredTagPoolSizeThenOneIsReturned) {
    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsr(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(TbxCommandStreamReceiver::create("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex())));
    EXPECT_EQ(1u, tbxCsr->getPreferredTagPoolSize());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenCreatedWithAubDumpThenFileNameIsExtendedWithSystemInfo) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.setHwInfo(*platformDevices);
    executionEnvironment.initializeMemoryManager();

    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    setMockAubCenter(*rootDeviceEnvironment, CommandStreamReceiverType::CSR_TBX);

    auto fullName = AUBCommandStreamReceiver::createFullFilePath(*platformDevices[0], "aubfile");

    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsr(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(TbxCommandStreamReceiver::create("aubfile", true, executionEnvironment, 0)));
    EXPECT_STREQ(fullName.c_str(), rootDeviceEnvironment->aubFileNameReceived.c_str());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenCreatedWithAubDumpThenOpenIsCalledOnAubManagerToOpenFileStream) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.setHwInfo(*platformDevices);
    executionEnvironment.initializeMemoryManager();

    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsrWithAubDump(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(
        TbxCommandStreamReceiver::create("aubfile", true, executionEnvironment, 0)));
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

    EXPECT_EQ(nullptr, executionEnvironment.rootDeviceEnvironments[expectedRootDeviceIndex]->aubCenter.get());
    EXPECT_FALSE(rootDeviceEnvironment->initAubCenterCalled);

    auto csr = std::make_unique<TbxCommandStreamReceiverHw<FamilyType>>(executionEnvironment, expectedRootDeviceIndex);
    EXPECT_NE(nullptr, executionEnvironment.rootDeviceEnvironments[expectedRootDeviceIndex]->aubCenter.get());
    EXPECT_TRUE(rootDeviceEnvironment->initAubCenterCalled);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenCreatedWithAubDumpInSubCaptureModeThenCreateSubCaptureManagerAndGenerateSubCaptureFileName) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpSubCaptureMode.set(static_cast<int32_t>(AubSubCaptureManager::SubCaptureMode::Filter));

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.setHwInfo(*platformDevices);
    executionEnvironment.initializeMemoryManager();

    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsrWithAubDump(static_cast<TbxCommandStreamReceiverHw<FamilyType> *>(
        TbxCommandStreamReceiver::create("aubfile", true, executionEnvironment, 0)));
    EXPECT_TRUE(tbxCsrWithAubDump->aubManager->isOpen());

    auto subCaptureManager = tbxCsrWithAubDump->subCaptureManager.get();
    EXPECT_NE(nullptr, subCaptureManager);

    MultiDispatchInfo dispatchInfo;
    EXPECT_STREQ(subCaptureManager->getSubCaptureFileName(dispatchInfo).c_str(), tbxCsrWithAubDump->aubManager->getFileName().c_str());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenCreatedWithAubDumpSeveralTimesThenOpenIsCalledOnAubManagerOnceOnly) {
    MockExecutionEnvironment executionEnvironment(*platformDevices, true, 1);
    executionEnvironment.setHwInfo(*platformDevices);
    executionEnvironment.initializeMemoryManager();

    auto tbxCsrWithAubDump1 = std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>>(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(
        TbxCommandStreamReceiverHw<FamilyType>::create("aubfile", true, executionEnvironment, 0)));

    auto tbxCsrWithAubDump2 = std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>>(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(
        TbxCommandStreamReceiverHw<FamilyType>::create("aubfile", true, executionEnvironment, 0)));

    auto mockManager = reinterpret_cast<MockAubManager *>(executionEnvironment.rootDeviceEnvironments[0]->aubCenter->getAubManager());
    EXPECT_EQ(1u, mockManager->openCalledCnt);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrInSubCaptureModeWhenFlushIsCalledAndSubCaptureIsDisabledThenPauseShouldBeTurnedOn) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment};
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    tbxCsr.setupContext(osContext);

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    tbxCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    EXPECT_FALSE(tbxCsr.subCaptureManager->isSubCaptureEnabled());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    ResidencyContainer allocationsForResidency = {};

    tbxCsr.flush(batchBuffer, allocationsForResidency);

    auto mockAubManager = reinterpret_cast<MockAubManager *>(pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter->getAubManager());
    EXPECT_TRUE(mockAubManager->isPaused);

    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrInSubCaptureModeWhenFlushIsCalledAndSubCaptureIsEnabledThenPauseShouldBeTurnedOff) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment};
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    tbxCsr.setupContext(osContext);

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureManagerMock->setSubCaptureIsActive(true);
    tbxCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    EXPECT_TRUE(tbxCsr.subCaptureManager->isSubCaptureEnabled());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    ResidencyContainer allocationsForResidency = {};

    tbxCsr.flush(batchBuffer, allocationsForResidency);

    auto mockAubManager = reinterpret_cast<MockAubManager *>(pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter->getAubManager());
    EXPECT_FALSE(mockAubManager->isPaused);

    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrInSubCaptureModeWhenFlushIsCalledAndSubCaptureIsEnabledThenCallPollForCompletionAndDisableSubCapture) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment};
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    tbxCsr.setupContext(osContext);

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureManagerMock->setSubCaptureIsActive(true);
    tbxCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    EXPECT_TRUE(tbxCsr.subCaptureManager->isSubCaptureEnabled());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    ResidencyContainer allocationsForResidency = {};

    tbxCsr.flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(tbxCsr.pollForCompletionCalled);
    EXPECT_FALSE(tbxCsr.subCaptureManager->isSubCaptureEnabled());

    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrInSubCaptureModeWhenFlushIsCalledAndSubCaptureGetsActivatedThenCallSubmitBatchBufferWithOverrideRingBufferSetToTrue) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment};
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    tbxCsr.setupContext(osContext);

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureManagerMock->setSubCaptureIsActive(true);
    tbxCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    EXPECT_FALSE(aubSubCaptureManagerMock->wasSubCaptureActiveInPreviousEnqueue());
    EXPECT_TRUE(aubSubCaptureManagerMock->isSubCaptureActive());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    ResidencyContainer allocationsForResidency = {};

    tbxCsr.flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(tbxCsr.submitBatchBufferCalled);
    EXPECT_TRUE(tbxCsr.overrideRingHeadPassed);

    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrInSubCaptureModeWhenFlushIsCalledAndSubCaptureRemainsActiveThenCallSubmitBatchBufferWithOverrideRingBufferSetToTrue) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment};
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    tbxCsr.setupContext(osContext);

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureManagerMock->setSubCaptureWasActiveInPreviousEnqueue(true);
    aubSubCaptureManagerMock->setSubCaptureIsActive(true);
    tbxCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);
    EXPECT_TRUE(aubSubCaptureManagerMock->wasSubCaptureActiveInPreviousEnqueue());
    EXPECT_TRUE(aubSubCaptureManagerMock->isSubCaptureActive());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr};
    ResidencyContainer allocationsForResidency = {};

    tbxCsr.flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(tbxCsr.submitBatchBufferCalled);
    EXPECT_FALSE(tbxCsr.overrideRingHeadPassed);

    pDevice->executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenProcessResidencyIsCalledWithDumpTbxNonWritableFlagThenAllocationsForResidencyShouldBeMadeTbxWritable) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockTbxCsrToTestDumpTbxNonWritable<FamilyType>> tbxCsr(new MockTbxCsrToTestDumpTbxNonWritable<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex()));
    memoryManager.reset(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));
    tbxCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});
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
    std::unique_ptr<MockTbxCsrToTestDumpTbxNonWritable<FamilyType>> tbxCsr(new MockTbxCsrToTestDumpTbxNonWritable<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex()));
    memoryManager.reset(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));
    tbxCsr->setupContext(*pDevice->getDefaultEngine().osContext);

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});
    tbxCsr->setTbxWritable(false, *gfxAllocation);

    EXPECT_FALSE(tbxCsr->dumpTbxNonWritable);

    ResidencyContainer allocationsForResidency = {gfxAllocation};
    tbxCsr->processResidency(allocationsForResidency, 0u);

    EXPECT_FALSE(tbxCsr->isTbxWritable(*gfxAllocation));
    EXPECT_FALSE(tbxCsr->dumpTbxNonWritable);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrInSubCaptureModeWhenCheckAndActivateAubSubCaptureIsCalledAndSubCaptureIsInactiveThenDontForceDumpingAllocationsTbxNonWritable) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment};
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    tbxCsr.setupContext(osContext);

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    tbxCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);

    MockKernelWithInternals kernelInternals(*pClDevice);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);

    EXPECT_FALSE(tbxCsr.dumpTbxNonWritable);

    auto status = tbxCsr.checkAndActivateAubSubCapture(multiDispatchInfo);
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);

    EXPECT_FALSE(tbxCsr.dumpTbxNonWritable);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrInSubCaptureModeWhenCheckAndActivateAubSubCaptureIsCalledAndSubCaptureGetsActivatedThenForceDumpingAllocationsTbxNonWritable) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment};
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    tbxCsr.setupContext(osContext);

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManagerMock->setSubCaptureIsActive(false);
    aubSubCaptureManagerMock->setSubCaptureToggleActive(true);
    tbxCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);

    MockKernelWithInternals kernelInternals(*pClDevice);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);

    EXPECT_FALSE(tbxCsr.dumpTbxNonWritable);

    auto status = tbxCsr.checkAndActivateAubSubCapture(multiDispatchInfo);
    EXPECT_TRUE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);

    EXPECT_TRUE(tbxCsr.dumpTbxNonWritable);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrInSubCaptureModeWhenCheckAndActivateAubSubCaptureIsCalledAndSubCaptureRemainsActivatedThenDontForceDumpingAllocationsTbxNonWritable) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment};
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    tbxCsr.setupContext(osContext);

    AubSubCaptureCommon aubSubCaptureCommon;
    auto aubSubCaptureManagerMock = new AubSubCaptureManagerMock("", aubSubCaptureCommon);
    aubSubCaptureCommon.subCaptureMode = AubSubCaptureManager::SubCaptureMode::Toggle;
    aubSubCaptureManagerMock->setSubCaptureIsActive(true);
    aubSubCaptureManagerMock->setSubCaptureToggleActive(true);
    tbxCsr.subCaptureManager = std::unique_ptr<AubSubCaptureManagerMock>(aubSubCaptureManagerMock);

    MockKernelWithInternals kernelInternals(*pClDevice);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(kernel);

    EXPECT_FALSE(tbxCsr.dumpTbxNonWritable);

    auto status = tbxCsr.checkAndActivateAubSubCapture(multiDispatchInfo);
    EXPECT_TRUE(status.isActive);
    EXPECT_TRUE(status.wasActiveInPreviousEnqueue);

    EXPECT_FALSE(tbxCsr.dumpTbxNonWritable);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrInNonSubCaptureModeWhenCheckAndActivateAubSubCaptureIsCalledThenReturnStatusInactive) {
    MockTbxCsr<FamilyType> tbxCsr{*pDevice->executionEnvironment};
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    tbxCsr.setupContext(osContext);

    MultiDispatchInfo dispatchInfo;
    auto status = tbxCsr.checkAndActivateAubSubCapture(dispatchInfo);
    EXPECT_FALSE(status.isActive);
    EXPECT_FALSE(status.wasActiveInPreviousEnqueue);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenDispatchBlitEnqueueThenProcessCorrectly) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableBlitterOperationsSupport.set(1);
    DebugManager.flags.EnableBlitterOperationsForReadWriteBuffers.set(1);

    MockContext context(pClDevice);

    MockTbxCsr<FamilyType> tbxCsr0{*pDevice->executionEnvironment};
    tbxCsr0.initializeTagAllocation();
    MockTbxCsr<FamilyType> tbxCsr1{*pDevice->executionEnvironment};
    tbxCsr1.initializeTagAllocation();

    MockOsContext osContext0(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    tbxCsr0.setupContext(osContext0);
    EngineControl engineControl0{&tbxCsr0, &osContext0};

    MockOsContext osContext1(1, 1, aub_stream::ENGINE_BCS, PreemptionMode::Disabled, false);
    tbxCsr1.setupContext(osContext0);
    EngineControl engineControl1{&tbxCsr1, &osContext1};

    MockCommandQueueHw<FamilyType> cmdQ(&context, pClDevice, nullptr);
    cmdQ.gpgpuEngine = &engineControl0;
    cmdQ.bcsEngine = &engineControl1;

    cl_int error = CL_SUCCESS;
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, 0, 1, nullptr, error));

    uint32_t hostPtr = 0;
    error = cmdQ.enqueueWriteBuffer(buffer.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, error);
}

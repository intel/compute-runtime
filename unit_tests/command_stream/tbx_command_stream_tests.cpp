/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/ptr_math.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/command_stream/tbx_command_stream_receiver_hw.h"
#include "runtime/helpers/hardware_context_controller.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/memory_manager/memory_banks.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/mock_aub_center_fixture.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/mocks/mock_aub_center.h"
#include "unit_tests/mocks/mock_aub_manager.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_os_context.h"
#include "unit_tests/mocks/mock_tbx_csr.h"

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
        setMockAubCenter(pDevice->getExecutionEnvironment());
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

TEST_F(TbxCommandStreamTests, DISABLED_testFactory) {
}

HWTEST_F(TbxCommandStreamTests, DISABLED_testTbxMemoryManager) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    TbxMemoryManager *getMM = tbxCsr->getMemoryManager();
    EXPECT_NE(nullptr, getMM);
    EXPECT_EQ(1 * GB, getMM->getSystemSharedMemory());
}

TEST_F(TbxCommandStreamTests, DISABLED_makeResident) {
    uint8_t buffer[0x10000];
    size_t size = sizeof(buffer);

    GraphicsAllocation *graphicsAllocation = mmTbx->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, size}, buffer);
    pCommandStreamReceiver->makeResident(*graphicsAllocation);
    pCommandStreamReceiver->makeNonResident(*graphicsAllocation);
    mmTbx->freeGraphicsMemory(graphicsAllocation);
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), startOffset, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    pCommandStreamReceiver->flush(batchBuffer, pCommandStreamReceiver->getResidencyAllocations());
}

HWTEST_F(TbxCommandStreamTests, DISABLED_flushUntilTailRingBufferLargerThanSizeRingBuffer) {
    char buffer[4096];
    memset(buffer, 0, 4096);
    LinearStream cs(buffer, 4096);
    size_t startOffset = 0;
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), startOffset, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
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
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    GFXCORE_FAMILY family = executionEnvironment->getHardwareInfo()->platform.eRenderCoreFamily;
    VariableBackup<TbxCommandStreamReceiverCreateFunc> tbxCsrFactoryBackup(&tbxCommandStreamReceiverFactory[family]);

    tbxCommandStreamReceiverFactory[family] = nullptr;

    CommandStreamReceiver *csr = TbxCommandStreamReceiver::create("", false, *executionEnvironment);
    EXPECT_EQ(nullptr, csr);
}

TEST(TbxCommandStreamReceiverTest, givenTbxCommandStreamReceiverWhenItIsCreatedWithWrongGfxCoreFamilyThenNullPointerShouldBeReturned) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    auto hwInfo = executionEnvironment->getMutableHardwareInfo();

    hwInfo->platform.eRenderCoreFamily = GFXCORE_FAMILY_FORCE_ULONG; // wrong gfx core family

    CommandStreamReceiver *csr = TbxCommandStreamReceiver::create("", false, *executionEnvironment);
    EXPECT_EQ(nullptr, csr);
}

TEST(TbxCommandStreamReceiverTest, givenTbxCommandStreamReceiverWhenTypeIsCheckedThenTbxCsrIsReturned) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    std::unique_ptr<CommandStreamReceiver> csr(TbxCommandStreamReceiver::create("", false, *executionEnvironment));
    EXPECT_NE(nullptr, csr);
    EXPECT_EQ(CommandStreamReceiverType::CSR_TBX, csr->getType());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenMakeResidentIsCalledForGraphicsAllocationThenItShouldPushAllocationForResidencyToCsr) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    TbxMemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_EQ(0u, tbxCsr->getResidencyAllocations().size());

    tbxCsr->makeResident(*graphicsAllocation);

    EXPECT_EQ(1u, tbxCsr->getResidencyAllocations().size());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenMakeResidentHasAlreadyBeenCalledForGraphicsAllocationThenItShouldNotPushAllocationForResidencyAgainToCsr) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    TbxMemoryManager *memoryManager = tbxCsr->getMemoryManager();
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
    TbxMemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_TRUE(tbxCsr->writeMemory(*graphicsAllocation));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenWriteMemoryIsCalledWithGraphicsAllocationThatIsOnlyOneTimeWriteableThenGraphicsAllocationIsUpdated) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    TbxMemoryManager *memoryManager = tbxCsr->getMemoryManager();
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
    TbxMemoryManager *memoryManager = tbxCsr->getMemoryManager();
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
    TbxMemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_FALSE(graphicsAllocation->isResident(tbxCsr->getOsContext().getContextId()));

    ResidencyContainer allocationsForResidency = {graphicsAllocation};
    tbxCsr->processResidency(allocationsForResidency);

    EXPECT_TRUE(graphicsAllocation->isResident(tbxCsr->getOsContext().getContextId()));
    EXPECT_EQ(tbxCsr->peekTaskCount() + 1, graphicsAllocation->getResidencyTaskCount(tbxCsr->getOsContext().getContextId()));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenProcessResidencyIsCalledWithAllocationsForResidencyThenItShouldProcessGivenAllocations) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    TbxMemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_FALSE(graphicsAllocation->isResident(tbxCsr->getOsContext().getContextId()));

    ResidencyContainer allocationsForResidency = {graphicsAllocation};
    tbxCsr->processResidency(allocationsForResidency);

    EXPECT_TRUE(graphicsAllocation->isResident(tbxCsr->getOsContext().getContextId()));
    EXPECT_EQ(tbxCsr->peekTaskCount() + 1, graphicsAllocation->getResidencyTaskCount(tbxCsr->getOsContext().getContextId()));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenFlushIsCalledThenItShouldProcessAllocationsForResidency) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    TbxMemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, graphicsAllocation);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

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
    TbxMemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

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

TEST(TbxMemoryManagerTest, givenTbxMemoryManagerWhenItIsQueriedForSystemSharedMemoryThen1GBIsReturned) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    TbxMemoryManager memoryManager(executionEnvironment);
    EXPECT_EQ(1 * GB, memoryManager.getSystemSharedMemory());
}

HWTEST_F(TbxCommandStreamTests, givenNoDbgDeviceIdFlagWhenTbxCsrIsCreatedThenUseDefaultDeviceId) {
    const HardwareInfo &hwInfo = *platformDevices[0];
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(pCommandStreamReceiver);
    EXPECT_EQ(hwInfo.capabilityTable.aubDeviceId, tbxCsr->aubDeviceId);
}

HWTEST_F(TbxCommandStreamTests, givenDbgDeviceIdFlagIsSetWhenTbxCsrIsCreatedThenUseDebugDeviceId) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.OverrideAubDeviceId.set(9); //this is Hsw, not used
    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsr(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(TbxCommandStreamReceiver::create("", false, *pDevice->executionEnvironment)));
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

    MockTbxCsr tbxCsr{*pDevice->executionEnvironment};
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
    MockExecutionEnvironment executionEnvironment(platformDevices[0], false);

    auto tbxCsr = std::make_unique<TbxCommandStreamReceiverHw<FamilyType>>(executionEnvironment);
    EXPECT_EQ(nullptr, executionEnvironment.aubCenter->getAubManager());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenFlushIsCalledThenItShouldCallTheExpectedHwContextFunctions) {
    MockTbxCsr<FamilyType> tbxCsr(*pDevice->executionEnvironment);
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    tbxCsr.setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(tbxCsr.hardwareContextController->hardwareContexts[0].get());

    auto commandBuffer = pDevice->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 1, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 1, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
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
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
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
    tbxCsr.processResidency(allocationsForResidency);

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

    pDevice->executionEnvironment->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);
    auto tbxCsr = std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>>(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(
        TbxCommandStreamReceiverHw<FamilyType>::create("", false, *pDevice->executionEnvironment)));

    EXPECT_FALSE(tbxCsr->streamInitialized);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenOsContextIsSetThenCreateHardwareContext) {
    auto hwInfo = pDevice->executionEnvironment->getHardwareInfo();
    MockOsContext osContext(0, 1, HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances()[0],
                            PreemptionMode::Disabled, false);
    std::string fileName = "";
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(hwInfo, false, fileName, CommandStreamReceiverType::CSR_TBX);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);

    pDevice->executionEnvironment->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsr(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(TbxCommandStreamReceiver::create(fileName, false, *pDevice->executionEnvironment)));
    EXPECT_EQ(nullptr, tbxCsr->hardwareContextController.get());

    tbxCsr->setupContext(osContext);
    EXPECT_NE(nullptr, tbxCsr->hardwareContextController.get());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenPollForCompletionImplIsCalledThenSimulatedCsrMethodIsCalled) {
    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsr(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(TbxCommandStreamReceiver::create("", false, *pDevice->executionEnvironment)));
    tbxCsr->pollForCompletionImpl();
}
HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenItIsQueriedForPreferredTagPoolSizeThenOneIsReturned) {
    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsr(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(TbxCommandStreamReceiver::create("", false, *pDevice->executionEnvironment)));
    EXPECT_EQ(1u, tbxCsr->getPreferredTagPoolSize());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenCreatedWithAubDumpThenFileNameIsExtendedWithSystemInfo) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.setHwInfo(*platformDevices);

    setMockAubCenter(&executionEnvironment, CommandStreamReceiverType::CSR_TBX);

    auto fullName = AUBCommandStreamReceiver::createFullFilePath(*platformDevices[0], "aubfile");

    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsr(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(TbxCommandStreamReceiver::create("aubfile", true, executionEnvironment)));
    EXPECT_STREQ(fullName.c_str(), executionEnvironment.aubFileNameReceived.c_str());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenCreatedWithAubDumpThenOpenIsCalledOnAubManagerToOpenFileStream) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.setHwInfo(*platformDevices);

    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsrWithAubDump(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(
        TbxCommandStreamReceiver::create("aubfile", true, executionEnvironment)));
    EXPECT_TRUE(tbxCsrWithAubDump->aubManager->isOpen());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenCreatedWithAubDumpSeveralTimesThenOpenIsCalledOnAubManagerOnceOnly) {
    MockExecutionEnvironment executionEnvironment(*platformDevices, true);
    executionEnvironment.setHwInfo(*platformDevices);

    auto tbxCsrWithAubDump1 = std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>>(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(
        TbxCommandStreamReceiverHw<FamilyType>::create("aubfile", true, executionEnvironment)));

    auto tbxCsrWithAubDump2 = std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>>(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(
        TbxCommandStreamReceiverHw<FamilyType>::create("aubfile", true, executionEnvironment)));

    auto mockManager = reinterpret_cast<MockAubManager *>(executionEnvironment.aubCenter->getAubManager());
    EXPECT_EQ(1u, mockManager->openCalledCnt);
}

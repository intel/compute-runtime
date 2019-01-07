/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/mem_obj/mem_obj.h"
#include "tbx_command_stream_fixture.h"
#include "runtime/command_stream/tbx_command_stream_receiver_hw.h"
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/memory_manager/memory_banks.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/os_context.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_aub_center.h"
#include "unit_tests/mocks/mock_aub_manager.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_tbx_csr.h"
#include "test.h"
#include <cstdint>

using namespace OCLRT;
namespace Os {
extern const char *tbxLibName;
}

struct TbxFixture : public TbxCommandStreamFixture,
                    public DeviceFixture {

    using TbxCommandStreamFixture::SetUp;

    void SetUp() {
        DeviceFixture::SetUp();
        TbxCommandStreamFixture::SetUp(pDevice);
    }

    void TearDown() override {
        TbxCommandStreamFixture::TearDown();
        DeviceFixture::TearDown();
    }
};

typedef Test<TbxFixture> TbxCommandStreamTests;
typedef Test<DeviceFixture> TbxCommandSteamSimpleTest;

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

    GraphicsAllocation *graphicsAllocation = mmTbx->allocateGraphicsMemory(MockAllocationProperties{false, size}, buffer);
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
    auto &engineInfo = tbxCsr->engineInfoTable[EngineType::ENGINE_RCS];

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), startOffset, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    pCommandStreamReceiver->flush(batchBuffer, pCommandStreamReceiver->getResidencyAllocations());
    auto size = engineInfo.sizeRingBuffer;
    engineInfo.sizeRingBuffer = 64;
    pCommandStreamReceiver->flush(batchBuffer, pCommandStreamReceiver->getResidencyAllocations());
    pCommandStreamReceiver->flush(batchBuffer, pCommandStreamReceiver->getResidencyAllocations());
    pCommandStreamReceiver->flush(batchBuffer, pCommandStreamReceiver->getResidencyAllocations());
    engineInfo.sizeRingBuffer = size;
}

HWTEST_F(TbxCommandStreamTests, DISABLED_getCsTraits) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    tbxCsr->getCsTraits(EngineType::ENGINE_RCS);
    tbxCsr->getCsTraits(EngineType::ENGINE_BCS);
    tbxCsr->getCsTraits(EngineType::ENGINE_VCS);
    tbxCsr->getCsTraits(EngineType::ENGINE_VECS);
}

#if defined(__linux__)
namespace OCLRT {
TEST(TbxCommandStreamReceiverTest, createShouldReturnNullptrForEmptyEntryInFactory) {
    extern TbxCommandStreamReceiverCreateFunc tbxCommandStreamReceiverFactory[IGFX_MAX_PRODUCT];

    TbxCommandStreamReceiver tbx;
    const HardwareInfo *hwInfo = platformDevices[0];
    GFXCORE_FAMILY family = hwInfo->pPlatform->eRenderCoreFamily;
    auto pCreate = tbxCommandStreamReceiverFactory[family];

    tbxCommandStreamReceiverFactory[family] = nullptr;
    ExecutionEnvironment executionEnvironment;
    CommandStreamReceiver *csr = tbx.create(*hwInfo, false, executionEnvironment);
    EXPECT_EQ(nullptr, csr);

    tbxCommandStreamReceiverFactory[family] = pCreate;
}
} // namespace OCLRT
#endif

TEST(TbxCommandStreamReceiverTest, givenTbxCommandStreamReceiverWhenItIsCreatedWithWrongGfxCoreFamilyThenNullPointerShouldBeReturned) {
    HardwareInfo hwInfo = *platformDevices[0];
    GFXCORE_FAMILY family = hwInfo.pPlatform->eRenderCoreFamily;

    const_cast<PLATFORM *>(hwInfo.pPlatform)->eRenderCoreFamily = GFXCORE_FAMILY_FORCE_ULONG; // wrong gfx core family
    ExecutionEnvironment executionEnvironment;
    CommandStreamReceiver *csr = TbxCommandStreamReceiver::create(hwInfo, false, executionEnvironment);
    EXPECT_EQ(nullptr, csr);

    const_cast<PLATFORM *>(hwInfo.pPlatform)->eRenderCoreFamily = family;
}

TEST(TbxCommandStreamReceiverTest, givenTbxCommandStreamReceiverWhenTypeIsCheckedThenTbxCsrIsReturned) {
    HardwareInfo hwInfo = *platformDevices[0];
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<CommandStreamReceiver> csr(TbxCommandStreamReceiver::create(hwInfo, false, executionEnvironment));
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

TEST(TbxMemoryManagerTest, givenTbxMemoryManagerWhenItIsQueriedForSystemSharedMemoryThen1GBIsReturned) {
    ExecutionEnvironment executionEnvironment;
    TbxMemoryManager memoryManager(false, false, executionEnvironment);
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
    const HardwareInfo &hwInfoIn = *platformDevices[0];
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>> tbxCsr(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(TbxCommandStreamReceiver::create(hwInfoIn, false, executionEnvironment)));
    EXPECT_EQ(9u, tbxCsr->aubDeviceId);
}

HWTEST_F(TbxCommandSteamSimpleTest, givenTbxCsrWhenWaitBeforeMakeNonResidentWhenRequiredIsCalledWithBlockingFlagTrueThenFunctionStallsUntilMakeCoherentUpdatesTagAddress) {
    uint32_t tag = 0;
    MockTbxCsrToTestWaitBeforeMakingNonResident<FamilyType> tbxCsr(*platformDevices[0], *pDevice->executionEnvironment);

    tbxCsr.setTagAllocation(pDevice->getMemoryManager()->allocateGraphicsMemory(MockAllocationProperties{false, sizeof(tag)}, &tag));

    EXPECT_FALSE(tbxCsr.makeCoherentCalled);

    *tbxCsr.getTagAddress() = 3;
    tbxCsr.latestFlushedTaskCount = 6;

    tbxCsr.waitBeforeMakingNonResidentWhenRequired();

    EXPECT_TRUE(tbxCsr.makeCoherentCalled);
    EXPECT_EQ(6u, tag);
}

HWTEST_F(TbxCommandSteamSimpleTest, whenTbxCommandStreamReceiverIsCreatedThenPPGTTAndGGTTCreatedHavePhysicalAddressAllocatorSet) {
    MockTbxCsr<FamilyType> tbxCsr(*platformDevices[0], *pDevice->executionEnvironment);

    uintptr_t address = 0x20000;
    auto physicalAddress = tbxCsr.ppgtt->map(address, MemoryConstants::pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(0u, physicalAddress);

    physicalAddress = tbxCsr.ggtt->map(address, MemoryConstants::pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(0u, physicalAddress);
}

HWTEST_F(TbxCommandSteamSimpleTest, givenTbxCommandStreamReceiverWhenPhysicalAddressAllocatorIsCreatedThenItIsNotNull) {
    MockTbxCsr<FamilyType> tbxCsr(*platformDevices[0], *pDevice->executionEnvironment);
    auto oldSkuTable = hwInfoHelper.pSkuTable;
    std::unique_ptr<FeatureTable, std::function<void(FeatureTable *)>> skuTable(new FeatureTable, [&](FeatureTable *ptr) { delete ptr;  hwInfoHelper.pSkuTable = oldSkuTable; });
    hwInfoHelper.pSkuTable = skuTable.get();
    std::unique_ptr<PhysicalAddressAllocator> allocator(tbxCsr.createPhysicalAddressAllocator(&hwInfoHelper));
    ASSERT_NE(nullptr, allocator);
    hwInfoHelper.pSkuTable = nullptr;
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenItIsCreatedWithUseAubStreamFalseThenDontInitializeAubManager) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.UseAubStream.set(false);

    const HardwareInfo &hwInfo = *platformDevices[0];
    ExecutionEnvironment executionEnvironment;
    auto tbxCsr = std::make_unique<TbxCommandStreamReceiverHw<FamilyType>>(hwInfo, executionEnvironment);
    EXPECT_EQ(nullptr, executionEnvironment.aubCenter->getAubManager());
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenItIsCreatedWithAubManagerThenCreateHardwareContext) {
    const HardwareInfo &hwInfo = *platformDevices[0];
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(&hwInfo, false, "");
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);
    auto tbxCsr = std::make_unique<TbxCommandStreamReceiverHw<FamilyType>>(hwInfo, executionEnvironment);
    EXPECT_NE(nullptr, tbxCsr->hardwareContext);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenFlushIsCalledThenItShouldCallTheExpectedHwContextFunctions) {
    auto mockManager = std::make_unique<MockAubManager>();
    auto mockHardwareContext = static_cast<MockHardwareContext *>(mockManager->createHardwareContext(0, EngineType::ENGINE_RCS));

    auto tbxExecutionEnvironment = getEnvironment<MockTbxCsr<FamilyType>>(true, true);
    auto tbxCsr = tbxExecutionEnvironment->template getCsr<MockTbxCsr<FamilyType>>();
    tbxCsr->hardwareContext = std::unique_ptr<MockHardwareContext>(mockHardwareContext);

    LinearStream cs(tbxExecutionEnvironment->commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 1, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    ResidencyContainer allocationsForResidency = {&allocation};

    tbxCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(mockHardwareContext->initializeCalled);
    EXPECT_TRUE(mockHardwareContext->writeMemoryCalled);
    EXPECT_TRUE(mockHardwareContext->submitCalled);
    EXPECT_TRUE(mockHardwareContext->pollForCompletionCalled);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverInBatchedModeWhenFlushIsCalledThenItShouldMakeCommandBufferResident) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::BatchedDispatch));

    auto mockManager = std::make_unique<MockAubManager>();
    auto mockHardwareContext = static_cast<MockHardwareContext *>(mockManager->createHardwareContext(0, EngineType::ENGINE_RCS));

    auto tbxExecutionEnvironment = getEnvironment<MockTbxCsr<FamilyType>>(true, true);
    auto tbxCsr = tbxExecutionEnvironment->template getCsr<MockTbxCsr<FamilyType>>();
    tbxCsr->hardwareContext = std::unique_ptr<MockHardwareContext>(mockHardwareContext);

    LinearStream cs(tbxExecutionEnvironment->commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 1, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    ResidencyContainer allocationsForResidency;

    tbxCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_TRUE(mockHardwareContext->writeMemoryCalled);
    EXPECT_EQ(1u, batchBuffer.commandBufferAllocation->getResidencyTaskCount(tbxCsr->getOsContext().getContextId()));
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenFlushIsCalledWithZeroSizedBufferThenSubmitIsNotCalledOnHwContext) {
    auto mockManager = std::make_unique<MockAubManager>();
    auto mockHardwareContext = static_cast<MockHardwareContext *>(mockManager->createHardwareContext(0, EngineType::ENGINE_RCS));

    auto tbxExecutionEnvironment = getEnvironment<MockTbxCsr<FamilyType>>(true, true);
    auto tbxCsr = tbxExecutionEnvironment->template getCsr<MockTbxCsr<FamilyType>>();
    tbxCsr->hardwareContext = std::unique_ptr<MockHardwareContext>(mockHardwareContext);

    LinearStream cs(tbxExecutionEnvironment->commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    ResidencyContainer allocationsForResidency;

    tbxCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_FALSE(mockHardwareContext->submitCalled);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenMakeResidentIsCalledThenItShouldCallTheExpectedHwContextFunctions) {
    auto mockManager = std::make_unique<MockAubManager>();
    auto mockHardwareContext = static_cast<MockHardwareContext *>(mockManager->createHardwareContext(0, EngineType::ENGINE_RCS));

    auto tbxExecutionEnvironment = getEnvironment<MockTbxCsr<FamilyType>>(true, true);
    auto tbxCsr = tbxExecutionEnvironment->template getCsr<MockTbxCsr<FamilyType>>();
    tbxCsr->hardwareContext = std::unique_ptr<MockHardwareContext>(mockHardwareContext);

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    ResidencyContainer allocationsForResidency = {&allocation};
    tbxCsr->processResidency(allocationsForResidency);

    EXPECT_TRUE(mockHardwareContext->writeMemoryCalled);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenMakeCoherentIsCalledThenItShouldCallTheExpectedHwContextFunctions) {
    auto mockManager = std::make_unique<MockAubManager>();
    auto mockHardwareContext = static_cast<MockHardwareContext *>(mockManager->createHardwareContext(0, EngineType::ENGINE_RCS));

    auto tbxExecutionEnvironment = getEnvironment<MockTbxCsr<FamilyType>>(true, true);
    auto tbxCsr = tbxExecutionEnvironment->template getCsr<MockTbxCsr<FamilyType>>();
    tbxCsr->hardwareContext = std::unique_ptr<MockHardwareContext>(mockHardwareContext);

    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    tbxCsr->makeCoherent(allocation);

    EXPECT_TRUE(mockHardwareContext->readMemoryCalled);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCsrWhenHardwareContextIsCreatedThenTbxStreamInCsrIsNotInitialized) {
    const HardwareInfo &hwInfo = *platformDevices[0];
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(&hwInfo, false, "");
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);
    auto tbxCsr = std::unique_ptr<TbxCommandStreamReceiverHw<FamilyType>>(reinterpret_cast<TbxCommandStreamReceiverHw<FamilyType> *>(
        TbxCommandStreamReceiverHw<FamilyType>::create(hwInfo, false, executionEnvironment)));

    EXPECT_FALSE(tbxCsr->streamInitialized);
}

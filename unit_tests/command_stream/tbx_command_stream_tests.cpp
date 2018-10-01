/*
 * Copyright (C) 2017-2018 Intel Corporation
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
#include "gen_cmd_parse.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
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

template <typename GfxFamily>
class MockTbxCsr : public TbxCommandStreamReceiverHw<GfxFamily> {
  public:
    using CommandStreamReceiver::latestFlushedTaskCount;
    MockTbxCsr(const HardwareInfo &hwInfoIn, ExecutionEnvironment &executionEnvironment)
        : TbxCommandStreamReceiverHw<GfxFamily>(hwInfoIn, executionEnvironment) {}

    void makeCoherent(GraphicsAllocation &gfxAllocation) override {
        auto tagAddress = reinterpret_cast<uint32_t *>(gfxAllocation.getUnderlyingBuffer());
        *tagAddress = this->latestFlushedTaskCount;
        makeCoherentCalled = true;
    }
    bool makeCoherentCalled = false;
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

    GraphicsAllocation *graphicsAllocation = mmTbx->allocateGraphicsMemory(size, buffer);
    pCommandStreamReceiver->makeResident(*graphicsAllocation);
    pCommandStreamReceiver->makeNonResident(*graphicsAllocation);
    mmTbx->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(TbxCommandStreamTests, DISABLED_makeResidentOnZeroSizedBufferShouldDoNothing) {
    GraphicsAllocation graphicsAllocation(nullptr, 0);

    pCommandStreamReceiver->makeResident(graphicsAllocation);
    pCommandStreamReceiver->makeNonResident(graphicsAllocation);
}

TEST_F(TbxCommandStreamTests, DISABLED_flush) {
    char buffer[4096];
    memset(buffer, 0, 4096);
    LinearStream cs(buffer, 4096);
    size_t startOffset = 0;
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), startOffset, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    pCommandStreamReceiver->flush(batchBuffer, EngineType::ENGINE_RCS, pCommandStreamReceiver->getResidencyAllocations(), *pDevice->getOsContext());
}

HWTEST_F(TbxCommandStreamTests, DISABLED_flushUntilTailRCSLargerThanSizeRCS) {
    char buffer[4096];
    memset(buffer, 0, 4096);
    LinearStream cs(buffer, 4096);
    size_t startOffset = 0;
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    auto &engineInfo = tbxCsr->engineInfoTable[EngineType::ENGINE_RCS];

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), startOffset, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    pCommandStreamReceiver->flush(batchBuffer, EngineType::ENGINE_RCS, pCommandStreamReceiver->getResidencyAllocations(), *pDevice->getOsContext());
    auto size = engineInfo.sizeRCS;
    engineInfo.sizeRCS = 64;
    pCommandStreamReceiver->flush(batchBuffer, EngineType::ENGINE_RCS, pCommandStreamReceiver->getResidencyAllocations(), *pDevice->getOsContext());
    pCommandStreamReceiver->flush(batchBuffer, EngineType::ENGINE_RCS, pCommandStreamReceiver->getResidencyAllocations(), *pDevice->getOsContext());
    pCommandStreamReceiver->flush(batchBuffer, EngineType::ENGINE_RCS, pCommandStreamReceiver->getResidencyAllocations(), *pDevice->getOsContext());
    engineInfo.sizeRCS = size;
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

    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(4096);
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

    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(4096);
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

    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_TRUE(tbxCsr->writeMemory(*graphicsAllocation));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenWriteMemoryIsCalledForGraphicsAllocationWithZeroSizeThenItShouldReturnFalse) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    GraphicsAllocation graphicsAllocation((void *)0x1234, 0);

    EXPECT_FALSE(tbxCsr->writeMemory(graphicsAllocation));
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenProcessResidencyIsCalledWithoutAllocationsForResidencyThenItShouldProcessAllocationsFromMemoryManager) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    TbxMemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_EQ(ObjectNotResident, graphicsAllocation->residencyTaskCount[0u]);

    ResidencyContainer allocationsForResidency = {graphicsAllocation};
    tbxCsr->processResidency(allocationsForResidency, *pDevice->getOsContext());

    EXPECT_NE(ObjectNotResident, graphicsAllocation->residencyTaskCount[0u]);
    EXPECT_EQ((int)tbxCsr->peekTaskCount() + 1, graphicsAllocation->residencyTaskCount[0u]);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenProcessResidencyIsCalledWithAllocationsForResidencyThenItShouldProcessGivenAllocations) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    TbxMemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_EQ(ObjectNotResident, graphicsAllocation->residencyTaskCount[0u]);

    ResidencyContainer allocationsForResidency = {graphicsAllocation};
    tbxCsr->processResidency(allocationsForResidency, *pDevice->getOsContext());

    EXPECT_NE(ObjectNotResident, graphicsAllocation->residencyTaskCount[0u]);
    EXPECT_EQ((int)tbxCsr->peekTaskCount() + 1, graphicsAllocation->residencyTaskCount[0u]);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(TbxCommandStreamTests, givenTbxCommandStreamReceiverWhenFlushIsCalledThenItShouldProcessAllocationsForResidency) {
    TbxCommandStreamReceiverHw<FamilyType> *tbxCsr = (TbxCommandStreamReceiverHw<FamilyType> *)pCommandStreamReceiver;
    TbxMemoryManager *memoryManager = tbxCsr->getMemoryManager();
    ASSERT_NE(nullptr, memoryManager);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    ASSERT_NE(nullptr, graphicsAllocation);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);

    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {graphicsAllocation};

    EXPECT_EQ(ObjectNotResident, graphicsAllocation->residencyTaskCount[0u]);

    tbxCsr->flush(batchBuffer, engineType, allocationsForResidency, *pDevice->getOsContext());

    EXPECT_NE(ObjectNotResident, graphicsAllocation->residencyTaskCount[0u]);
    EXPECT_EQ((int)tbxCsr->peekTaskCount() + 1, graphicsAllocation->residencyTaskCount[0u]);

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
    MockTbxCsr<FamilyType> tbxCsr(*platformDevices[0], *pDevice->executionEnvironment);
    GraphicsAllocation graphicsAllocation(&tag, sizeof(tag));
    tbxCsr.setTagAllocation(&graphicsAllocation);

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

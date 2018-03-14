/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/command_stream/aub_command_stream_receiver_hw.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_gmm.h"

using namespace OCLRT;

using ::testing::Invoke;
using ::testing::_;
using ::testing::Return;

typedef Test<DeviceFixture> AubCommandStreamReceiverTests;

template <typename GfxFamily>
struct MockAubCsr : public AUBCommandStreamReceiverHw<GfxFamily> {
    MockAubCsr(const HardwareInfo &hwInfoIn, bool standalone) : AUBCommandStreamReceiverHw<GfxFamily>(hwInfoIn, standalone){};

    CommandStreamReceiver::DispatchMode peekDispatchMode() const {
        return this->dispatchMode;
    }

    GraphicsAllocation *getTagAllocation() const {
        return this->tagAllocation;
    }

    void setLatestSentTaskCount(uint32_t latestSentTaskCount) {
        this->latestSentTaskCount = latestSentTaskCount;
    }

    MOCK_METHOD2(flattenBatchBuffer, void *(BatchBuffer &batchBuffer, size_t &sizeBatchBuffer));
    MOCK_METHOD0(addPatchInfoComments, bool(void));
};

struct MockAubFileStream : public AUBCommandStreamReceiver::AubFileStream {
    MOCK_METHOD1(addComment, bool(const char *message));
};

TEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenItIsCreatedWithWrongGfxCoreFamilyThenNullPointerShouldBeReturned) {
    HardwareInfo hwInfo = *platformDevices[0];
    GFXCORE_FAMILY family = hwInfo.pPlatform->eRenderCoreFamily;

    const_cast<PLATFORM *>(hwInfo.pPlatform)->eRenderCoreFamily = GFXCORE_FAMILY_FORCE_ULONG; // wrong gfx core family

    CommandStreamReceiver *aubCsr = AUBCommandStreamReceiver::create(hwInfo, "", true);
    EXPECT_EQ(nullptr, aubCsr);

    const_cast<PLATFORM *>(hwInfo.pPlatform)->eRenderCoreFamily = family;
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrWhenItIsCreatedWithDefaultSettingsThenItHasBatchedDispatchModeEnabled) {
    DebugManager.flags.CsrDispatchMode.set(0);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], true));
    EXPECT_EQ(CommandStreamReceiver::DispatchMode::BatchedDispatch, aubCsr->peekDispatchMode());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrWhenItIsCreatedWithDebugSettingsThenItHasProperDispatchModeEnabled) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.CsrDispatchMode.set(CommandStreamReceiver::DispatchMode::ImmediateDispatch);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], true));
    EXPECT_EQ(CommandStreamReceiver::DispatchMode::ImmediateDispatch, aubCsr->peekDispatchMode());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenItIsCreatedThenMemoryManagerIsNotNull) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(**platformDevices, true));
    std::unique_ptr<MemoryManager> memoryManager(aubCsr->createMemoryManager(false));
    EXPECT_NE(nullptr, memoryManager.get());
    aubCsr->setMemoryManager(nullptr);
}

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationWhenMakeResidentCalledMultipleTimesAffectsResidencyOnce) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));
    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    // First makeResident marks the allocation resident
    aubCsr->makeResident(*gfxAllocation);
    EXPECT_NE(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(1u, memoryManager->getResidencyAllocations().size());

    // Second makeResident should have no impact
    aubCsr->makeResident(*gfxAllocation);
    EXPECT_NE(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(1u, memoryManager->getResidencyAllocations().size());

    // First makeNonResident marks the allocation as nonresident
    aubCsr->makeNonResident(*gfxAllocation);
    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(1u, memoryManager->getEvictionAllocations().size());

    // Second makeNonResident should have no impact
    aubCsr->makeNonResident(*gfxAllocation);
    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(1u, memoryManager->getEvictionAllocations().size());

    memoryManager->freeGraphicsMemoryImpl(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenFlushIsCalledThenItShouldLeaveProperRingTailAlignment) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    aubCsr->setTagAllocation(pDevice->getTagAllocation());
    ASSERT_NE(nullptr, aubCsr->getTagAllocation());

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    auto engineType = OCLRT::ENGINE_RCS;
    auto ringTailAlignment = sizeof(uint64_t);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    // First flush typically includes a preamble and chain to command buffer
    aubCsr->overrideDispatchPolicy(CommandStreamReceiver::DispatchMode::ImmediateDispatch);
    aubCsr->flush(batchBuffer, engineType, nullptr);
    EXPECT_EQ(0ull, aubCsr->engineInfoTable[engineType].tailRingBuffer % ringTailAlignment);

    // Second flush should just submit command buffer
    cs.getSpace(sizeof(uint64_t));
    aubCsr->flush(batchBuffer, engineType, nullptr);
    EXPECT_EQ(0ull, aubCsr->engineInfoTable[engineType].tailRingBuffer % ringTailAlignment);

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneModeWhenFlushIsCalledThenItShouldUpdateHwTagWithLatestSentTaskCount) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    auto commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    aubCsr->setTagAllocation(pDevice->getTagAllocation());
    ASSERT_NE(nullptr, aubCsr->getTagAllocation());
    EXPECT_EQ(initialHardwareTag, *aubCsr->getTagAddress());

    aubCsr->setLatestSentTaskCount(aubCsr->peekTaskCount() + 1);

    EXPECT_NE(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);

    EXPECT_EQ(aubCsr->peekLatestSentTaskCount(), *aubCsr->getTagAddress());

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneModeWhenFlushIsCalledThenItShouldCallMakeResidentOnCommandBufferAllocation) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    aubCsr->setTagAllocation(pDevice->getTagAllocation());
    ASSERT_NE(nullptr, aubCsr->getTagAllocation());

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    aubCsr->overrideDispatchPolicy(CommandStreamReceiver::DispatchMode::ImmediateDispatch);
    aubCsr->flush(batchBuffer, engineType, nullptr);

    EXPECT_NE(ObjectNotResident, commandBuffer->residencyTaskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, commandBuffer->residencyTaskCount);

    aubCsr->makeSurfacePackNonResident(nullptr);

    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNoneStandaloneModeWhenFlushIsCalledThenItShouldNotCallMakeResidentOnCommandBufferAllocation) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], false));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    aubCsr->flush(batchBuffer, engineType, nullptr);

    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    memoryManager->freeGraphicsMemory(commandBuffer);
    aubCsr->setMemoryManager(nullptr);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInStandaloneModeWhenFlushIsCalledThenItShouldCallMakeResidentOnResidencyAllocations) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    aubCsr->setTagAllocation(pDevice->getTagAllocation());
    ASSERT_NE(nullptr, aubCsr->getTagAllocation());

    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    ASSERT_NE(nullptr, gfxAllocation);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {gfxAllocation};

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    aubCsr->overrideDispatchPolicy(CommandStreamReceiver::DispatchMode::BatchedDispatch);
    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);

    EXPECT_NE(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, gfxAllocation->residencyTaskCount);

    EXPECT_NE(ObjectNotResident, commandBuffer->residencyTaskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount() + 1, commandBuffer->residencyTaskCount);

    aubCsr->makeSurfacePackNonResident(&allocationsForResidency);

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverInNoneStandaloneModeWhenFlushIsCalledThenItShouldNotCallMakeResidentOnResidencyAllocations) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], false));
    memoryManager.reset(aubCsr->createMemoryManager(false));
    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {gfxAllocation};

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    memoryManager->freeGraphicsMemoryImpl(commandBuffer);
    memoryManager->freeGraphicsMemoryImpl(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationIsCreatedThenItDoesntHaveTypeNonAubWritable) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    EXPECT_FALSE(!!(gfxAllocation->getAllocationType() & GraphicsAllocation::ALLOCATION_TYPE_NON_AUB_WRITABLE));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledOnDefaultAllocationThenAllocationTypeShouldNotBeMadeNonAubWritable) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    auto gfxDefaultAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    ResidencyContainer allocationsForResidency = {gfxDefaultAllocation};
    aubCsr->processResidency(&allocationsForResidency);

    EXPECT_FALSE(!!(gfxDefaultAllocation->getAllocationType() & GraphicsAllocation::ALLOCATION_TYPE_NON_AUB_WRITABLE));

    memoryManager->freeGraphicsMemory(gfxDefaultAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledOnBufferAndImageAllocationsThenAllocationsTypesShouldBeMadeNonAubWritable) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    auto gfxBufferAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    gfxBufferAllocation->setAllocationType(GraphicsAllocation::ALLOCATION_TYPE_BUFFER);

    auto gfxImageAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    gfxImageAllocation->setAllocationType(GraphicsAllocation::ALLOCATION_TYPE_IMAGE);

    ResidencyContainer allocationsForResidency = {gfxBufferAllocation, gfxImageAllocation};
    aubCsr->processResidency(&allocationsForResidency);

    EXPECT_TRUE(!!(gfxBufferAllocation->getAllocationType() & GraphicsAllocation::ALLOCATION_TYPE_NON_AUB_WRITABLE));
    EXPECT_TRUE(!!(gfxImageAllocation->getAllocationType() & GraphicsAllocation::ALLOCATION_TYPE_NON_AUB_WRITABLE));

    memoryManager->freeGraphicsMemory(gfxBufferAllocation);
    memoryManager->freeGraphicsMemory(gfxImageAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationTypeIsntNonAubWritableThenWriteMemoryIsAllowed) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    EXPECT_TRUE(aubCsr->writeMemory(*gfxAllocation));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationTypeIsNonAubWritableThenWriteMemoryIsNotAllowed) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    gfxAllocation->setAllocationType(GraphicsAllocation::ALLOCATION_TYPE_NON_AUB_WRITABLE);
    EXPECT_FALSE(aubCsr->writeMemory(*gfxAllocation));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGraphicsAllocationSizeIsZeroThenWriteMemoryIsNotAllowed) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    GraphicsAllocation gfxAllocation((void *)0x1234, 0);

    EXPECT_FALSE(aubCsr->writeMemory(gfxAllocation));
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferFlatteningThenNewCombinedBatchBufferIsCreated) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    auto chainedBatchBuffer = memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    auto otherAllocation = memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    ASSERT_NE(nullptr, chainedBatchBuffer);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, chainedBatchBuffer, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<void, std::function<void(void *)>> flatBatchBuffer(aubCsr->flattenBatchBuffer(batchBuffer, sizeBatchBuffer), [&](void *ptr) { memoryManager->alignedFreeWrapper(ptr); });
    EXPECT_NE(nullptr, flatBatchBuffer.get());
    EXPECT_EQ(alignUp(128u + 128u, 0x1000), sizeBatchBuffer);

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(chainedBatchBuffer);
    memoryManager->freeGraphicsMemory(otherAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferAndNoChainedBatchBufferThenCombinedBatchBufferIsNotCreated) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<void, std::function<void(void *)>> flatBatchBuffer(aubCsr->flattenBatchBuffer(batchBuffer, sizeBatchBuffer), [&](void *ptr) { memoryManager->alignedFreeWrapper(ptr); });
    EXPECT_EQ(nullptr, flatBatchBuffer.get());
    EXPECT_EQ(0xffffu, sizeBatchBuffer);

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenDefaultDebugConfigThenExpectFlattenBatchBufferIsNotCalled) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    aubCsr->setTagAllocation(pDevice->getTagAllocation());
    ASSERT_NE(nullptr, aubCsr->getTagAllocation());

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {};

    EXPECT_CALL(*aubCsr, flattenBatchBuffer(::testing::_, ::testing::_)).Times(0);
    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedFlattenBatchBufferAndImmediateDispatchModeThenExpectFlattenBatchBufferIsCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    DebugManager.flags.CsrDispatchMode.set(CommandStreamReceiver::DispatchMode::ImmediateDispatch);

    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    aubCsr->setTagAllocation(pDevice->getTagAllocation());
    ASSERT_NE(nullptr, aubCsr->getTagAllocation());

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    auto chainedBatchBuffer = memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    ASSERT_NE(nullptr, chainedBatchBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, chainedBatchBuffer, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    aubCsr->makeResident(*chainedBatchBuffer);

    std::unique_ptr<void, decltype(alignedFree) *> ptr(alignedMalloc(4096, 4096), alignedFree);

    EXPECT_CALL(*aubCsr, flattenBatchBuffer(::testing::_, ::testing::_)).WillOnce(::testing::Return(ptr.release()));
    aubCsr->flush(batchBuffer, engineType, nullptr);

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(chainedBatchBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedFlattenBatchBufferAndImmediateDispatchModeAndThereIsNoChainedBatchBufferThenExpectFlattenBatchBufferIsCalledAnyway) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    DebugManager.flags.CsrDispatchMode.set(CommandStreamReceiver::DispatchMode::ImmediateDispatch);

    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    aubCsr->setTagAllocation(pDevice->getTagAllocation());
    ASSERT_NE(nullptr, aubCsr->getTagAllocation());

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    EXPECT_CALL(*aubCsr, flattenBatchBuffer(::testing::_, ::testing::_)).Times(1);
    aubCsr->flush(batchBuffer, engineType, nullptr);

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenDispatchModeIsNotImmediateThenExpectFlattenBatchBufferIsNotCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);

    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));
    aubCsr->overrideDispatchPolicy(CommandStreamReceiver::DispatchMode::BatchedDispatch);

    aubCsr->setTagAllocation(pDevice->getTagAllocation());
    ASSERT_NE(nullptr, aubCsr->getTagAllocation());

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    auto chainedBatchBuffer = memoryManager->allocateGraphicsMemory(128u, 64u, false, false);
    ASSERT_NE(nullptr, chainedBatchBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, chainedBatchBuffer, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    ResidencyContainer allocationsForResidency = {chainedBatchBuffer};

    EXPECT_CALL(*aubCsr, flattenBatchBuffer(::testing::_, ::testing::_)).Times(0);
    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(chainedBatchBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAddPatchInfoCommentsForAUBDumpIsSetThenAddPatchInfoCommentsIsCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency;
    aubCsr->setTagAllocation(pDevice->getTagAllocation());

    EXPECT_CALL(*aubCsr, addPatchInfoComments()).Times(1);
    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAddPatchInfoCommentsForAUBDumpIsNotSetThenAddPatchInfoCommentsIsNotCalled) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency;
    aubCsr->setTagAllocation(pDevice->getTagAllocation());

    EXPECT_CALL(*aubCsr, addPatchInfoComments()).Times(0);
    aubCsr->flush(batchBuffer, engineType, &allocationsForResidency);

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAddPatchInfoCommentsCalledWhenNoPatchInfoDataObjectsThenCommentsAreEmpty) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new MockAubFileStream());
    MockAubFileStream *mockAubFileStreamPtr = static_cast<MockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    mockAubFileStream.swap(aubCsr->stream);

    std::vector<std::string> comments;

    EXPECT_CALL(*mockAubFileStreamPtr, addComment(_)).Times(2).WillRepeatedly(::testing::Invoke([&](const char *str) -> bool {
        comments.push_back(std::string(str));
        return true;
    }));
    bool result = aubCsr->addPatchInfoComments();
    EXPECT_TRUE(result);

    ASSERT_EQ(2u, comments.size());

    EXPECT_EQ("PatchInfoData\n", comments[0]);
    EXPECT_EQ("AllocationsList\n", comments[1]);

    mockAubFileStream.swap(aubCsr->stream);
    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAddPatchInfoCommentsCalledWhenFirstAddCommentsFailsThenFunctionReturnsFalse) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new MockAubFileStream());
    MockAubFileStream *mockAubFileStreamPtr = static_cast<MockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    mockAubFileStream.swap(aubCsr->stream);

    EXPECT_CALL(*mockAubFileStreamPtr, addComment(_)).Times(1).WillOnce(Return(false));
    bool result = aubCsr->addPatchInfoComments();
    EXPECT_FALSE(result);

    mockAubFileStream.swap(aubCsr->stream);
    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAddPatchInfoCommentsCalledWhenSecondAddCommentsFailsThenFunctionReturnsFalse) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new MockAubFileStream());
    MockAubFileStream *mockAubFileStreamPtr = static_cast<MockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    mockAubFileStream.swap(aubCsr->stream);

    EXPECT_CALL(*mockAubFileStreamPtr, addComment(_)).Times(2).WillOnce(Return(true)).WillOnce(Return(false));
    bool result = aubCsr->addPatchInfoComments();
    EXPECT_FALSE(result);

    mockAubFileStream.swap(aubCsr->stream);
    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAddPatchInfoCommentsCalledWhenPatchInfoDataObjectsAddedThenCommentsAreNotEmpty) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new MockAubFileStream());
    MockAubFileStream *mockAubFileStreamPtr = static_cast<MockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    mockAubFileStream.swap(aubCsr->stream);

    PatchInfoData patchInfoData[2] = {{0xAAAAAAAA, 128u, PatchInfoAllocationType::Default, 0xBBBBBBBB, 256u, PatchInfoAllocationType::Default},
                                      {0xBBBBBBBB, 128u, PatchInfoAllocationType::Default, 0xDDDDDDDD, 256u, PatchInfoAllocationType::Default}};

    EXPECT_TRUE(aubCsr->setPatchInfoData(patchInfoData[0]));
    EXPECT_TRUE(aubCsr->setPatchInfoData(patchInfoData[1]));

    std::vector<std::string> comments;

    EXPECT_CALL(*mockAubFileStreamPtr, addComment(_)).Times(2).WillRepeatedly(::testing::Invoke([&](const char *str) -> bool {
        comments.push_back(std::string(str));
        return true;
    }));
    bool result = aubCsr->addPatchInfoComments();
    EXPECT_TRUE(result);

    ASSERT_EQ(2u, comments.size());

    EXPECT_EQ("PatchInfoData", comments[0].substr(0, 13));
    EXPECT_EQ("AllocationsList", comments[1].substr(0, 15));

    std::string line;
    std::istringstream input1;
    input1.str(comments[0]);

    uint32_t lineNo = 0;
    while (std::getline(input1, line)) {
        if (line.substr(0, 13) == "PatchInfoData") {
            continue;
        }
        std::ostringstream ss;
        ss << std::hex << patchInfoData[lineNo].sourceAllocation << ";" << patchInfoData[lineNo].sourceAllocationOffset << ";" << patchInfoData[lineNo].sourceType << ";";
        ss << patchInfoData[lineNo].targetAllocation << ";" << patchInfoData[lineNo].targetAllocationOffset << ";" << patchInfoData[lineNo].targetType << ";";

        EXPECT_EQ(ss.str(), line);
        lineNo++;
    }

    std::vector<std::string> expectedAddresses = {"aaaaaaaa", "bbbbbbbb", "cccccccc", "dddddddd"};
    lineNo = 0;

    std::istringstream input2;
    input2.str(comments[1]);
    while (std::getline(input2, line)) {
        if (line.substr(0, 15) == "AllocationsList") {
            continue;
        }

        bool foundAddr = false;
        for (auto &addr : expectedAddresses) {
            if (line.substr(0, 8) == addr) {
                foundAddr = true;
                break;
            }
        }
        EXPECT_TRUE(foundAddr);
        EXPECT_TRUE(line.size() > 9);
        lineNo++;
    }

    mockAubFileStream.swap(aubCsr->stream);
    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAddPatchInfoCommentsCalledWhenSourceAllocationIsNullThenDoNotAddToAllocationsList) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new MockAubFileStream());
    MockAubFileStream *mockAubFileStreamPtr = static_cast<MockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    mockAubFileStream.swap(aubCsr->stream);

    PatchInfoData patchInfoData = {0x0, 0u, PatchInfoAllocationType::Default, 0xBBBBBBBB, 0u, PatchInfoAllocationType::Default};
    EXPECT_TRUE(aubCsr->setPatchInfoData(patchInfoData));

    std::vector<std::string> comments;

    EXPECT_CALL(*mockAubFileStreamPtr, addComment(_)).Times(2).WillRepeatedly(::testing::Invoke([&](const char *str) -> bool {
        comments.push_back(std::string(str));
        return true;
    }));
    bool result = aubCsr->addPatchInfoComments();
    EXPECT_TRUE(result);

    ASSERT_EQ(2u, comments.size());

    ASSERT_EQ("PatchInfoData", comments[0].substr(0, 13));
    ASSERT_EQ("AllocationsList", comments[1].substr(0, 15));

    std::string line;
    std::istringstream input;
    input.str(comments[1]);

    uint32_t lineNo = 0;

    std::vector<std::string> expectedAddresses = {"bbbbbbbb"};
    while (std::getline(input, line)) {
        if (line.substr(0, 15) == "AllocationsList") {
            continue;
        }

        bool foundAddr = false;
        for (auto &addr : expectedAddresses) {
            if (line.substr(0, 8) == addr) {
                foundAddr = true;
                break;
            }
        }
        EXPECT_TRUE(foundAddr);
        EXPECT_TRUE(line.size() > 9);
        lineNo++;
    }

    mockAubFileStream.swap(aubCsr->stream);
    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAddPatchInfoCommentsCalledWhenTargetAllocationIsNullThenDoNotAddToAllocationsList) {
    std::unique_ptr<MemoryManager> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(aubCsr->createMemoryManager(false));

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> mockAubFileStream(new MockAubFileStream());
    MockAubFileStream *mockAubFileStreamPtr = static_cast<MockAubFileStream *>(mockAubFileStream.get());
    ASSERT_NE(nullptr, mockAubFileStreamPtr);
    mockAubFileStream.swap(aubCsr->stream);

    PatchInfoData patchInfoData = {0xAAAAAAAA, 0u, PatchInfoAllocationType::Default, 0x0, 0u, PatchInfoAllocationType::Default};
    EXPECT_TRUE(aubCsr->setPatchInfoData(patchInfoData));

    std::vector<std::string> comments;

    EXPECT_CALL(*mockAubFileStreamPtr, addComment(_)).Times(2).WillRepeatedly(::testing::Invoke([&](const char *str) -> bool {
        comments.push_back(std::string(str));
        return true;
    }));
    bool result = aubCsr->addPatchInfoComments();
    EXPECT_TRUE(result);

    ASSERT_EQ(2u, comments.size());

    ASSERT_EQ("PatchInfoData", comments[0].substr(0, 13));
    ASSERT_EQ("AllocationsList", comments[1].substr(0, 15));

    std::string line;
    std::istringstream input;
    input.str(comments[1]);

    uint32_t lineNo = 0;

    std::vector<std::string> expectedAddresses = {"aaaaaaaa"};
    while (std::getline(input, line)) {
        if (line.substr(0, 15) == "AllocationsList") {
            continue;
        }

        bool foundAddr = false;
        for (auto &addr : expectedAddresses) {
            if (line.substr(0, 8) == addr) {
                foundAddr = true;
                break;
            }
        }
        EXPECT_TRUE(foundAddr);
        EXPECT_TRUE(line.size() > 9);
        lineNo++;
    }

    mockAubFileStream.swap(aubCsr->stream);
    memoryManager->freeGraphicsMemory(commandBuffer);
}

class OsAgnosticMemoryManagerForImagesWithNoHostPtr : public OsAgnosticMemoryManager {
  public:
    GraphicsAllocation *allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) override {
        auto imageAllocation = OsAgnosticMemoryManager::allocateGraphicsMemoryForImage(imgInfo, gmm);
        cpuPtr = imageAllocation->getUnderlyingBuffer();
        imageAllocation->setCpuPtrAndGpuAddress(nullptr, imageAllocation->getGpuAddress());
        return imageAllocation;
    };
    void freeGraphicsMemoryImpl(GraphicsAllocation *imageAllocation) override {
        imageAllocation->setCpuPtrAndGpuAddress(lockResourceParam.retCpuPtr, imageAllocation->getGpuAddress());
        OsAgnosticMemoryManager::freeGraphicsMemoryImpl(imageAllocation);
    };
    void *lockResource(GraphicsAllocation *imageAllocation) override {
        lockResourceParam.wasCalled = true;
        lockResourceParam.inImageAllocation = imageAllocation;
        lockResourceParam.retCpuPtr = cpuPtr;
        return lockResourceParam.retCpuPtr;
    };
    void unlockResource(GraphicsAllocation *imageAllocation) override {
        unlockResourceParam.wasCalled = true;
        unlockResourceParam.inImageAllocation = imageAllocation;
    };

    struct LockResourceParam {
        bool wasCalled = false;
        GraphicsAllocation *inImageAllocation = nullptr;
        void *retCpuPtr = nullptr;
    } lockResourceParam;
    struct UnlockResourceParam {
        bool wasCalled = false;
        GraphicsAllocation *inImageAllocation = nullptr;
    } unlockResourceParam;

  protected:
    void *cpuPtr = nullptr;
};

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenWriteMemoryIsCalledOnImageWithNoHostPtrThenResourceShouldBeLockedToGetCpuAddress) {
    std::unique_ptr<OsAgnosticMemoryManagerForImagesWithNoHostPtr> memoryManager(nullptr);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0], true));
    memoryManager.reset(new OsAgnosticMemoryManagerForImagesWithNoHostPtr);
    aubCsr->setMemoryManager(memoryManager.get());

    cl_image_desc imgDesc = {};
    imgDesc.image_width = 512;
    imgDesc.image_height = 1;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    auto queryGmm = MockGmm::queryImgParams(imgInfo);

    auto imageAllocation = memoryManager->allocateGraphicsMemoryForImage(imgInfo, queryGmm.get());
    ASSERT_NE(nullptr, imageAllocation);

    EXPECT_TRUE(aubCsr->writeMemory(*imageAllocation));

    EXPECT_TRUE(memoryManager->lockResourceParam.wasCalled);
    EXPECT_EQ(imageAllocation, memoryManager->lockResourceParam.inImageAllocation);
    EXPECT_NE(nullptr, memoryManager->lockResourceParam.retCpuPtr);

    EXPECT_TRUE(memoryManager->unlockResourceParam.wasCalled);
    EXPECT_EQ(imageAllocation, memoryManager->unlockResourceParam.inImageAllocation);

    queryGmm.release();
    memoryManager->freeGraphicsMemory(imageAllocation);
}

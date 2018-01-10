/*
 * Copyright (c) 2017, Intel Corporation
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
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"

using OCLRT::AUBCommandStreamReceiver;
using OCLRT::AUBCommandStreamReceiverHw;
using OCLRT::BatchBuffer;
using OCLRT::CommandStreamReceiver;
using OCLRT::GraphicsAllocation;
using OCLRT::ResidencyContainer;
using OCLRT::HardwareInfo;
using OCLRT::LinearStream;
using OCLRT::MemoryManager;
using OCLRT::ObjectNotResident;
using OCLRT::platformDevices;

typedef Test<DeviceFixture> AubCommandStreamReceiverTests;

TEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenItIsCreatedWithWrongGfxCoreFamilyThenNullPointerShouldBeReturned) {
    HardwareInfo hwInfo = *platformDevices[0];
    GFXCORE_FAMILY family = hwInfo.pPlatform->eRenderCoreFamily;

    const_cast<PLATFORM *>(hwInfo.pPlatform)->eRenderCoreFamily = GFXCORE_FAMILY_FORCE_ULONG; // wrong gfx core family

    CommandStreamReceiver *csr = AUBCommandStreamReceiver::create(hwInfo, "");
    EXPECT_EQ(nullptr, csr);

    const_cast<PLATFORM *>(hwInfo.pPlatform)->eRenderCoreFamily = family;
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenItIsCreatedThenMemoryManagerIsNotNull) {
    HardwareInfo hwInfo;

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(hwInfo));
    std::unique_ptr<MemoryManager> memoryManager(aubCsr->createMemoryManager(false));
    EXPECT_NE(nullptr, memoryManager.get());
    aubCsr->setMemoryManager(nullptr);
}

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationWhenMakeResidentCalledMultipleTimesAffectsResidencyOnce) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0]));
    std::unique_ptr<MemoryManager> memoryManager(aubCsr->createMemoryManager(false));
    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    // First makeResident marks the allocation resident
    aubCsr->makeResident(*gfxAllocation);
    EXPECT_NE(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount(), gfxAllocation->residencyTaskCount);
    EXPECT_EQ(1u, memoryManager->getResidencyAllocations().size());

    // Second makeResident should have no impact
    aubCsr->makeResident(*gfxAllocation);
    EXPECT_NE(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ((int)aubCsr->peekTaskCount(), gfxAllocation->residencyTaskCount);
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
    aubCsr->setMemoryManager(nullptr);
}

HWTEST_F(AubCommandStreamReceiverTests, flushShouldLeaveProperRingTailAlignment) {
    auto csr = new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0]);
    auto mm = csr->createMemoryManager(false);

    GraphicsAllocation *commandBuffer = mm->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    auto engineOrdinal = OCLRT::ENGINE_RCS;
    auto ringTailAlignment = sizeof(uint64_t);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, false, false, cs.getUsed(), &cs};

    // First flush typically includes a preamble and chain to command buffer
    csr->flush(batchBuffer, engineOrdinal, nullptr);
    EXPECT_EQ(0ull, csr->engineInfoTable[engineOrdinal].tailRingBuffer % ringTailAlignment);

    // Second flush should just submit command buffer
    cs.getSpace(sizeof(uint64_t));
    csr->flush(batchBuffer, engineOrdinal, nullptr);
    EXPECT_EQ(0ull, csr->engineInfoTable[engineOrdinal].tailRingBuffer % ringTailAlignment);

    mm->freeGraphicsMemory(commandBuffer);
    delete csr;
    delete mm;
}

HWTEST_F(AubCommandStreamReceiverTests, flushShouldCallMakeResidentOnCommandBufferAllocation) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> csr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0]));
    std::unique_ptr<MemoryManager> mm(csr->createMemoryManager(false));

    GraphicsAllocation *commandBuffer = mm->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, false, false, cs.getUsed(), &cs};
    auto engineOrdinal = OCLRT::ENGINE_RCS;

    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    csr->overrideDispatchPolicy(CommandStreamReceiver::DispatchMode::ImmediateDispatch);
    csr->flush(batchBuffer, engineOrdinal, nullptr);

    EXPECT_NE(ObjectNotResident, commandBuffer->residencyTaskCount);
    EXPECT_EQ((int)csr->peekTaskCount(), commandBuffer->residencyTaskCount);

    csr->makeSurfacePackNonResident(nullptr);

    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    mm->freeGraphicsMemoryImpl(commandBuffer);
    csr->setMemoryManager(nullptr);
}

HWTEST_F(AubCommandStreamReceiverTests, flushShouldCallMakeResidentOnResidencyAllocations) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> csr(new AUBCommandStreamReceiverHw<FamilyType>(*platformDevices[0]));
    std::unique_ptr<MemoryManager> mm(csr->createMemoryManager(false));
    auto gfxAllocation = mm->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);

    GraphicsAllocation *commandBuffer = mm->allocateGraphicsMemory(4096, 4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, false, false, cs.getUsed(), &cs};
    auto engineOrdinal = OCLRT::ENGINE_RCS;
    ResidencyContainer allocationsForResidency = {gfxAllocation};

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    csr->overrideDispatchPolicy(CommandStreamReceiver::DispatchMode::BatchedDispatch);
    csr->flush(batchBuffer, engineOrdinal, &allocationsForResidency);

    EXPECT_NE(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ((int)csr->peekTaskCount(), gfxAllocation->residencyTaskCount);

    EXPECT_NE(ObjectNotResident, commandBuffer->residencyTaskCount);
    EXPECT_EQ((int)csr->peekTaskCount(), commandBuffer->residencyTaskCount);

    csr->makeSurfacePackNonResident(&allocationsForResidency);

    EXPECT_EQ(ObjectNotResident, gfxAllocation->residencyTaskCount);
    EXPECT_EQ(ObjectNotResident, commandBuffer->residencyTaskCount);

    mm->freeGraphicsMemory(commandBuffer);
    mm->freeGraphicsMemoryImpl(gfxAllocation);
    csr->setMemoryManager(nullptr);
}

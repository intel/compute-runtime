/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/aub_mem_dump/aub_alloc_dump.h"
#include "runtime/aub_mem_dump/page_table_entry_bits.h"
#include "runtime/helpers/hardware_context_controller.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/mem_obj/mem_obj_helper.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/fixtures/aub_command_stream_receiver_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/mock_aub_center_fixture.h"
#include "unit_tests/mocks/mock_aub_center.h"
#include "unit_tests/mocks/mock_aub_csr.h"
#include "unit_tests/mocks/mock_aub_file_stream.h"
#include "unit_tests/mocks/mock_aub_manager.h"
#include "unit_tests/mocks/mock_aub_subcapture_manager.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"
#include "unit_tests/mocks/mock_os_context.h"

#include "third_party/aub_stream/headers/aubstream.h"

using namespace NEO;

using AubCommandStreamReceiverTests = Test<AubCommandStreamReceiverFixture>;

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferFlatteningInImmediateDispatchModeThenNewCombinedBatchBufferIsCreated) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment));
    std::unique_ptr<MemoryManager> memoryManager(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));
    auto flatBatchBufferHelper = new FlatBatchBufferHelperHw<FamilyType>(*pDevice->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(flatBatchBufferHelper);

    auto chainedBatchBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{128u});
    auto otherAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{128u});
    ASSERT_NE(nullptr, chainedBatchBuffer);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{128u});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, chainedBatchBuffer, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(batchBuffer, sizeBatchBuffer, DispatchMode::ImmediateDispatch),
        [&](GraphicsAllocation *ptr) { memoryManager->freeGraphicsMemory(ptr); });
    EXPECT_NE(nullptr, flatBatchBuffer->getUnderlyingBuffer());
    EXPECT_EQ(alignUp(128u + 128u, MemoryConstants::pageSize), sizeBatchBuffer);

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(chainedBatchBuffer);
    memoryManager->freeGraphicsMemory(otherAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferInImmediateDispatchModeAndNoChainedBatchBufferThenCombinedBatchBufferIsNotCreated) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment));
    std::unique_ptr<MemoryManager> memoryManager(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));
    auto flatBatchBufferHelper = new FlatBatchBufferHelperHw<FamilyType>(*pDevice->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(flatBatchBufferHelper);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(batchBuffer, sizeBatchBuffer, DispatchMode::ImmediateDispatch),
        [&](GraphicsAllocation *ptr) { memoryManager->freeGraphicsMemory(ptr); });
    EXPECT_EQ(nullptr, flatBatchBuffer.get());
    EXPECT_EQ(0xffffu, sizeBatchBuffer);

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferAndNotImmediateOrBatchedDispatchModeThenCombinedBatchBufferIsNotCreated) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment));
    std::unique_ptr<MemoryManager> memoryManager(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));
    auto flatBatchBufferHelper = new FlatBatchBufferHelperHw<FamilyType>(*pDevice->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(flatBatchBufferHelper);

    auto chainedBatchBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    auto otherAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, chainedBatchBuffer);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, chainedBatchBuffer, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(batchBuffer, sizeBatchBuffer, DispatchMode::AdaptiveDispatch),
        [&](GraphicsAllocation *ptr) { memoryManager->freeGraphicsMemory(ptr); });
    EXPECT_EQ(nullptr, flatBatchBuffer.get());
    EXPECT_EQ(0xffffu, sizeBatchBuffer);

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(chainedBatchBuffer);
    memoryManager->freeGraphicsMemory(otherAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenRegisterCommandChunkIsCalledThenNewChunkIsAddedToTheList) {
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(batchBuffer, sizeof(MI_BATCH_BUFFER_START));
    ASSERT_EQ(1u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList().size());
    EXPECT_EQ(128u + sizeof(MI_BATCH_BUFFER_START), aubCsr->getFlatBatchBufferHelper().getCommandChunkList()[0].endOffset);

    CommandChunk chunk;
    chunk.endOffset = 0x123;
    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(chunk);

    ASSERT_EQ(2u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList().size());
    EXPECT_EQ(0x123u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList()[1].endOffset);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenRemovePatchInfoDataIsCalledThenElementIsRemovedFromPatchInfoList) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();

    PatchInfoData patchInfoData(0xA000, 0x0, PatchInfoAllocationType::KernelArg, 0xB000, 0x0, PatchInfoAllocationType::Default);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData);
    EXPECT_EQ(1u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());

    EXPECT_TRUE(aubCsr->getFlatBatchBufferHelper().removePatchInfoData(0xC000));
    EXPECT_EQ(1u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());

    EXPECT_TRUE(aubCsr->getFlatBatchBufferHelper().removePatchInfoData(0xB000));
    EXPECT_EQ(0u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAddGucStartMessageIsCalledThenBatchBufferAddressIsStoredInPatchInfoCollection) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, false, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    std::unique_ptr<char> batchBuffer(new char[1024]);
    aubCsr->addGUCStartMessage(static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(batchBuffer.get())));

    auto &patchInfoCollection = aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection();
    ASSERT_EQ(1u, patchInfoCollection.size());
    EXPECT_EQ(patchInfoCollection[0].sourceAllocation, reinterpret_cast<uint64_t>(batchBuffer.get()));
    EXPECT_EQ(patchInfoCollection[0].targetType, PatchInfoAllocationType::GUCStartMessage);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferFlatteningInBatchedDispatchModeThenNewCombinedBatchBufferIsCreated) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::BatchedDispatch));

    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    auto memoryManager = aubExecutionEnvironment->executionEnvironment->memoryManager.get();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    CommandChunk chunk1;
    CommandChunk chunk2;
    CommandChunk chunk3;

    std::unique_ptr<char> commands1(new char[0x100u]);
    commands1.get()[0] = 0x1;
    chunk1.baseAddressCpu = chunk1.baseAddressGpu = reinterpret_cast<uint64_t>(commands1.get());
    chunk1.startOffset = 0u;
    chunk1.endOffset = 0x50u;

    std::unique_ptr<char> commands2(new char[0x100u]);
    commands2.get()[0] = 0x2;
    chunk2.baseAddressCpu = chunk2.baseAddressGpu = reinterpret_cast<uint64_t>(commands2.get());
    chunk2.startOffset = 0u;
    chunk2.endOffset = 0x50u;
    aubCsr->getFlatBatchBufferHelper().registerBatchBufferStartAddress(reinterpret_cast<uint64_t>(commands2.get() + 0x40), reinterpret_cast<uint64_t>(commands1.get()));

    std::unique_ptr<char> commands3(new char[0x100u]);
    commands3.get()[0] = 0x3;
    chunk3.baseAddressCpu = chunk3.baseAddressGpu = reinterpret_cast<uint64_t>(commands3.get());
    chunk3.startOffset = 0u;
    chunk3.endOffset = 0x50u;
    aubCsr->getFlatBatchBufferHelper().registerBatchBufferStartAddress(reinterpret_cast<uint64_t>(commands3.get() + 0x40), reinterpret_cast<uint64_t>(commands2.get()));

    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(chunk1);
    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(chunk2);
    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(chunk3);

    ASSERT_EQ(3u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList().size());

    PatchInfoData patchInfoData1(0xAAAu, 0xAu, PatchInfoAllocationType::IndirectObjectHeap, chunk1.baseAddressGpu, 0x10, PatchInfoAllocationType::Default);
    PatchInfoData patchInfoData2(0xBBBu, 0xAu, PatchInfoAllocationType::IndirectObjectHeap, chunk1.baseAddressGpu, 0x60, PatchInfoAllocationType::Default);
    PatchInfoData patchInfoData3(0xCCCu, 0xAu, PatchInfoAllocationType::IndirectObjectHeap, 0x0, 0x10, PatchInfoAllocationType::Default);

    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData1);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData2);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData3);

    ASSERT_EQ(3u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    size_t sizeBatchBuffer = 0u;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(batchBuffer, sizeBatchBuffer, DispatchMode::BatchedDispatch),
        [&](GraphicsAllocation *ptr) { memoryManager->freeGraphicsMemory(ptr); });

    EXPECT_NE(nullptr, flatBatchBuffer.get());
    EXPECT_EQ(alignUp(0x50u + 0x40u + 0x40u + CSRequirements::csOverfetchSize, 0x1000u), sizeBatchBuffer);

    ASSERT_EQ(1u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());
    EXPECT_EQ(0xAAAu, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection()[0].sourceAllocation);

    EXPECT_EQ(0u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList().size());

    EXPECT_EQ(0x3, static_cast<char *>(flatBatchBuffer->getUnderlyingBuffer())[0]);
    EXPECT_EQ(0x2, static_cast<char *>(flatBatchBuffer->getUnderlyingBuffer())[0x40]);
    EXPECT_EQ(0x1, static_cast<char *>(flatBatchBuffer->getUnderlyingBuffer())[0x40 + 0x40]);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenDefaultDebugConfigThenExpectFlattenBatchBufferIsNotCalled) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*aubExecutionEnvironment->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    ResidencyContainer allocationsForResidency = {};

    EXPECT_CALL(*mockHelper, flattenBatchBuffer(::testing::_, ::testing::_, ::testing::_)).Times(0);
    aubCsr->flush(batchBuffer, allocationsForResidency);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedFlattenBatchBufferAndImmediateDispatchModeThenExpectFlattenBatchBufferIsCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::ImmediateDispatch));

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    auto allocationsForResidency = aubCsr->getResidencyAllocations();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*aubExecutionEnvironment->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);

    auto chainedBatchBuffer = aubExecutionEnvironment->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, chainedBatchBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, chainedBatchBuffer, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    aubCsr->makeResident(*chainedBatchBuffer);

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> ptr(
        aubExecutionEnvironment->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize}),
        [&](GraphicsAllocation *ptr) { aubExecutionEnvironment->executionEnvironment->memoryManager->freeGraphicsMemory(ptr); });

    auto expectedAllocation = ptr.get();
    EXPECT_CALL(*mockHelper, flattenBatchBuffer(::testing::_, ::testing::_, ::testing::_)).WillOnce(::testing::Return(ptr.release()));
    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_EQ(batchBuffer.commandBufferAllocation, expectedAllocation);

    aubExecutionEnvironment->executionEnvironment->memoryManager->freeGraphicsMemory(chainedBatchBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedFlattenBatchBufferAndImmediateDispatchModeAndThereIsNoChainedBatchBufferThenExpectFlattenBatchBufferIsCalledAnyway) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::ImmediateDispatch));

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    auto allocationsForResidency = aubCsr->getResidencyAllocations();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*aubExecutionEnvironment->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    EXPECT_CALL(*mockHelper, flattenBatchBuffer(::testing::_, ::testing::_, ::testing::_)).Times(1);
    aubCsr->flush(batchBuffer, allocationsForResidency);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedFlattenBatchBufferAndBatchedDispatchModeThenExpectFlattenBatchBufferIsCalledAnyway) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::BatchedDispatch));

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*aubExecutionEnvironment->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);
    ResidencyContainer allocationsForResidency;

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    EXPECT_CALL(*mockHelper, flattenBatchBuffer(::testing::_, ::testing::_, ::testing::_)).Times(1);
    aubCsr->flush(batchBuffer, allocationsForResidency);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAddPatchInfoCommentsForAUBDumpIsSetThenAddPatchInfoCommentsIsCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    ResidencyContainer allocationsForResidency;

    EXPECT_CALL(*aubCsr, addPatchInfoComments()).Times(1);
    aubCsr->flush(batchBuffer, allocationsForResidency);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAddPatchInfoCommentsForAUBDumpIsNotSetThenAddPatchInfoCommentsIsNotCalled) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    ResidencyContainer allocationsForResidency;

    EXPECT_CALL(*aubCsr, addPatchInfoComments()).Times(0);

    aubCsr->flush(batchBuffer, allocationsForResidency);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGetIndirectPatchCommandsIsCalledForEmptyPatchInfoListThenIndirectPatchCommandBufferIsNotCreated) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, false, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();

    size_t indirectPatchCommandsSize = 0u;
    std::vector<PatchInfoData> indirectPatchInfo;

    std::unique_ptr<char> commandBuffer(aubCsr->getFlatBatchBufferHelper().getIndirectPatchCommands(indirectPatchCommandsSize, indirectPatchInfo));
    EXPECT_EQ(0u, indirectPatchCommandsSize);
    EXPECT_EQ(0u, indirectPatchInfo.size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGetIndirectPatchCommandsIsCalledForNonEmptyPatchInfoListThenIndirectPatchCommandBufferIsCreated) {
    typedef typename FamilyType::MI_STORE_DATA_IMM MI_STORE_DATA_IMM;
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment));

    PatchInfoData patchInfo1(0xA000, 0u, PatchInfoAllocationType::KernelArg, 0x6000, 0x100, PatchInfoAllocationType::IndirectObjectHeap);
    PatchInfoData patchInfo2(0xB000, 0u, PatchInfoAllocationType::KernelArg, 0x6000, 0x200, PatchInfoAllocationType::IndirectObjectHeap);
    PatchInfoData patchInfo3(0xC000, 0u, PatchInfoAllocationType::IndirectObjectHeap, 0x1000, 0x100, PatchInfoAllocationType::Default);
    PatchInfoData patchInfo4(0xC000, 0u, PatchInfoAllocationType::Default, 0x2000, 0x100, PatchInfoAllocationType::GUCStartMessage);

    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo1);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo2);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo3);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo4);

    size_t indirectPatchCommandsSize = 0u;
    std::vector<PatchInfoData> indirectPatchInfo;

    std::unique_ptr<char> commandBuffer(aubCsr->getFlatBatchBufferHelper().getIndirectPatchCommands(indirectPatchCommandsSize, indirectPatchInfo));
    EXPECT_EQ(4u, indirectPatchInfo.size());
    EXPECT_EQ(2u * sizeof(MI_STORE_DATA_IMM), indirectPatchCommandsSize);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAddBatchBufferStartCalledAndBatchBUfferFlatteningEnabledThenBatchBufferStartAddressIsRegistered) {
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment));

    MI_BATCH_BUFFER_START bbStart;

    aubCsr->addBatchBufferStart(&bbStart, 0xA000u, false);
    std::map<uint64_t, uint64_t> &batchBufferStartAddressSequence = aubCsr->getFlatBatchBufferHelper().getBatchBufferStartAddressSequence();

    ASSERT_EQ(1u, batchBufferStartAddressSequence.size());
    std::pair<uint64_t, uint64_t> addr = *batchBufferStartAddressSequence.begin();
    EXPECT_EQ(reinterpret_cast<uint64_t>(&bbStart), addr.first);
    EXPECT_EQ(0xA000u, addr.second);
}

class OsAgnosticMemoryManagerForImagesWithNoHostPtr : public OsAgnosticMemoryManager {
  public:
    OsAgnosticMemoryManagerForImagesWithNoHostPtr(ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(executionEnvironment) {}

    GraphicsAllocation *allocateGraphicsMemoryForImage(const AllocationData &allocationData) override {
        auto imageAllocation = OsAgnosticMemoryManager::allocateGraphicsMemoryForImage(allocationData);
        cpuPtr = imageAllocation->getUnderlyingBuffer();
        imageAllocation->setCpuPtrAndGpuAddress(nullptr, imageAllocation->getGpuAddress());
        return imageAllocation;
    };
    void freeGraphicsMemoryImpl(GraphicsAllocation *imageAllocation) override {
        imageAllocation->setCpuPtrAndGpuAddress(cpuPtr, imageAllocation->getGpuAddress());
        OsAgnosticMemoryManager::freeGraphicsMemoryImpl(imageAllocation);
    };
    void *lockResourceImpl(GraphicsAllocation &imageAllocation) override {
        lockResourceParam.wasCalled = true;
        lockResourceParam.inImageAllocation = &imageAllocation;
        lockCpuPtr = alignedMalloc(imageAllocation.getUnderlyingBufferSize(), MemoryConstants::pageSize);
        lockResourceParam.retCpuPtr = lockCpuPtr;
        return lockResourceParam.retCpuPtr;
    };
    void unlockResourceImpl(GraphicsAllocation &imageAllocation) override {
        unlockResourceParam.wasCalled = true;
        unlockResourceParam.inImageAllocation = &imageAllocation;
        alignedFree(lockCpuPtr);
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
    void *lockCpuPtr = nullptr;
};

using AubCommandStreamReceiverNoHostPtrTests = ::testing::Test;
HWTEST_F(AubCommandStreamReceiverNoHostPtrTests, givenAubCommandStreamReceiverWhenWriteMemoryIsCalledOnImageWithNoHostPtrThenResourceShouldBeLockedToGetCpuAddress) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    auto memoryManager = new OsAgnosticMemoryManagerForImagesWithNoHostPtr(*executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    auto engineInstance = HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances()[0];

    MockOsContext osContext(0, 1, engineInstance, PreemptionMode::Disabled, false);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *executionEnvironment));
    aubCsr->setupContext(osContext);

    cl_image_desc imgDesc = {};
    imgDesc.image_width = 512;
    imgDesc.image_height = 1;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    AllocationProperties allocProperties = MemObjHelper::getAllocationPropertiesWithImageInfo(imgInfo, true, 0);

    auto imageAllocation = memoryManager->allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);
    ASSERT_NE(nullptr, imageAllocation);

    EXPECT_TRUE(aubCsr->writeMemory(*imageAllocation));

    EXPECT_TRUE(memoryManager->lockResourceParam.wasCalled);
    EXPECT_EQ(imageAllocation, memoryManager->lockResourceParam.inImageAllocation);
    EXPECT_NE(nullptr, memoryManager->lockResourceParam.retCpuPtr);

    EXPECT_TRUE(memoryManager->unlockResourceParam.wasCalled);
    EXPECT_EQ(imageAllocation, memoryManager->unlockResourceParam.inImageAllocation);
    memoryManager->freeGraphicsMemory(imageAllocation);
}

HWTEST_F(AubCommandStreamReceiverNoHostPtrTests, givenAubCommandStreamReceiverWhenWriteMemoryIsCalledOnLockedResourceThenResourceShouldNotBeUnlocked) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    auto memoryManager = new OsAgnosticMemoryManagerForImagesWithNoHostPtr(*executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *executionEnvironment));
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    memoryManager->lockResource(gfxAllocation);
    EXPECT_TRUE(aubCsr->writeMemory(*gfxAllocation));

    EXPECT_FALSE(memoryManager->unlockResourceParam.wasCalled);
    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenNoDbgDeviceIdFlagWhenAubCsrIsCreatedThenUseDefaultDeviceId) {
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment));
    EXPECT_EQ(pDevice->executionEnvironment->getHardwareInfo()->capabilityTable.aubDeviceId, aubCsr->aubDeviceId);
}

HWTEST_F(AubCommandStreamReceiverTests, givenDbgDeviceIdFlagIsSetWhenAubCsrIsCreatedThenUseDebugDeviceId) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.OverrideAubDeviceId.set(9); //this is Hsw, not used
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment));
    EXPECT_EQ(9u, aubCsr->aubDeviceId);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGetGTTDataIsCalledThenLocalMemoryIsSetAccordingToCsrFeature) {
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment));
    AubGTTData data = {};
    aubCsr->getGTTData(nullptr, data);
    EXPECT_TRUE(data.present);

    if (aubCsr->localMemoryEnabled) {
        EXPECT_TRUE(data.localMemory);
    } else {
        EXPECT_FALSE(data.localMemory);
    }
}

HWTEST_F(AubCommandStreamReceiverTests, givenPhysicalAddressWhenSetGttEntryIsCalledThenGttEntrysBitFieldsShouldBePopulated) {
    typedef typename AUBFamilyMapper<FamilyType>::AUB AUB;

    AubMemDump::MiGttEntry entry = {};
    uint64_t address = 0x0123456789;
    AubGTTData data = {true, false};
    AUB::setGttEntry(entry, address, data);

    EXPECT_EQ(entry.pageConfig.PhysicalAddress, address / 4096);
    EXPECT_TRUE(entry.pageConfig.Present);
    EXPECT_FALSE(entry.pageConfig.LocalMemory);
}

HWTEST_F(AubCommandStreamReceiverTests, whenGetMemoryBankForGttIsCalledThenCorrectBankIsReturned) {
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment));
    aubCsr->localMemoryEnabled = false;

    auto bank = aubCsr->getMemoryBankForGtt();
    EXPECT_EQ(MemoryBanks::MainBank, bank);
}

HWTEST_F(AubCommandStreamReceiverTests, givenEntryBitsPresentAndWritableWhenGetAddressSpaceFromPTEBitsIsCalledThenTraceNonLocalIsReturned) {
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment));

    auto space = aubCsr->getAddressSpaceFromPTEBits(PageTableEntry::presentBit | PageTableEntry::writableBit);
    EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceNonlocal, space);
}

template <typename GfxFamily>
struct MockAubCsrToTestExternalAllocations : public AUBCommandStreamReceiverHw<GfxFamily> {
    using AUBCommandStreamReceiverHw<GfxFamily>::AUBCommandStreamReceiverHw;
    using AUBCommandStreamReceiverHw<GfxFamily>::externalAllocations;

    bool writeMemory(AllocationView &allocationView) override {
        writeMemoryParametrization.wasCalled = true;
        writeMemoryParametrization.receivedAllocationView = allocationView;
        writeMemoryParametrization.statusToReturn = (0 != allocationView.second) ? true : false;
        return writeMemoryParametrization.statusToReturn;
    }
    struct WriteMemoryParametrization {
        bool wasCalled = false;
        AllocationView receivedAllocationView = {};
        bool statusToReturn = false;
    } writeMemoryParametrization;
};

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMakeResidentExternalIsCalledThenGivenAllocationViewShouldBeAddedToExternalAllocations) {
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>("", true, *pDevice->executionEnvironment);
    size_t size = 100;
    auto ptr = std::make_unique<char[]>(size);
    auto addr = reinterpret_cast<uint64_t>(ptr.get());
    AllocationView externalAllocation(addr, size);

    ASSERT_EQ(0u, aubCsr->externalAllocations.size());
    aubCsr->makeResidentExternal(externalAllocation);
    EXPECT_EQ(1u, aubCsr->externalAllocations.size());
    EXPECT_EQ(addr, aubCsr->externalAllocations[0].first);
    EXPECT_EQ(size, aubCsr->externalAllocations[0].second);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMakeNonResidentExternalIsCalledThenMatchingAllocationViewShouldBeRemovedFromExternalAllocations) {
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>("", true, *pDevice->executionEnvironment);
    size_t size = 100;
    auto ptr = std::make_unique<char[]>(size);
    auto addr = reinterpret_cast<uint64_t>(ptr.get());
    AllocationView externalAllocation(addr, size);
    aubCsr->makeResidentExternal(externalAllocation);

    ASSERT_EQ(1u, aubCsr->externalAllocations.size());
    aubCsr->makeNonResidentExternal(addr);
    EXPECT_EQ(0u, aubCsr->externalAllocations.size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenMakeNonResidentExternalIsCalledThenNonMatchingAllocationViewShouldNotBeRemovedFromExternalAllocations) {
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>("", true, *pDevice->executionEnvironment);
    size_t size = 100;
    auto ptr = std::make_unique<char[]>(size);
    auto addr = reinterpret_cast<uint64_t>(ptr.get());
    AllocationView externalAllocation(addr, size);
    aubCsr->makeResidentExternal(externalAllocation);

    ASSERT_EQ(1u, aubCsr->externalAllocations.size());
    aubCsr->makeNonResidentExternal(0);
    EXPECT_EQ(1u, aubCsr->externalAllocations.size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledThenExternalAllocationsShouldBeMadeResident) {
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>("", true, *pDevice->executionEnvironment);
    size_t size = 100;
    auto ptr = std::make_unique<char[]>(size);
    auto addr = reinterpret_cast<uint64_t>(ptr.get());
    AllocationView externalAllocation(addr, size);
    aubCsr->makeResidentExternal(externalAllocation);

    ASSERT_EQ(1u, aubCsr->externalAllocations.size());
    ResidencyContainer allocationsForResidency;
    aubCsr->processResidency(allocationsForResidency);

    EXPECT_TRUE(aubCsr->writeMemoryParametrization.wasCalled);
    EXPECT_EQ(addr, aubCsr->writeMemoryParametrization.receivedAllocationView.first);
    EXPECT_EQ(size, aubCsr->writeMemoryParametrization.receivedAllocationView.second);
    EXPECT_TRUE(aubCsr->writeMemoryParametrization.statusToReturn);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledThenExternalAllocationWithZeroSizeShouldNotBeMadeResident) {
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>("", true, *pDevice->executionEnvironment);
    AllocationView externalAllocation(0, 0);
    aubCsr->makeResidentExternal(externalAllocation);

    ASSERT_EQ(1u, aubCsr->externalAllocations.size());
    ResidencyContainer allocationsForResidency;
    aubCsr->processResidency(allocationsForResidency);

    EXPECT_TRUE(aubCsr->writeMemoryParametrization.wasCalled);
    EXPECT_EQ(0u, aubCsr->writeMemoryParametrization.receivedAllocationView.first);
    EXPECT_EQ(0u, aubCsr->writeMemoryParametrization.receivedAllocationView.second);
    EXPECT_FALSE(aubCsr->writeMemoryParametrization.statusToReturn);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenWriteMemoryIsCalledThenGraphicsAllocationSizeIsReadCorrectly) {
    pDevice->executionEnvironment->aubCenter.reset(new AubCenter());

    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", false, *pDevice->executionEnvironment);
    std::unique_ptr<MemoryManager> memoryManager(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));

    PhysicalAddressAllocator allocator;
    struct PpgttMock : std::conditional<is64bit, PML4, PDPE>::type {
        PpgttMock(PhysicalAddressAllocator *allocator) : std::conditional<is64bit, PML4, PDPE>::type(allocator) {}

        void pageWalk(uintptr_t vm, size_t size, size_t offset, uint64_t entryBits, PageWalker &pageWalker, uint32_t memoryBank) override {
            receivedSize = size;
        }
        size_t receivedSize = 0;
    };
    auto ppgttMock = new PpgttMock(&allocator);

    aubCsr->ppgtt.reset(ppgttMock);

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    aubCsr->setAubWritable(true, *gfxAllocation);

    auto gmm = new Gmm(nullptr, 1, false);
    gfxAllocation->setDefaultGmm(gmm);

    for (bool compressed : {false, true}) {
        gmm->isRenderCompressed = compressed;

        aubCsr->writeMemory(*gfxAllocation);

        if (compressed) {
            EXPECT_EQ(gfxAllocation->getDefaultGmm()->gmmResourceInfo->getSizeAllocation(), ppgttMock->receivedSize);
        } else {
            EXPECT_EQ(gfxAllocation->getUnderlyingBufferSize(), ppgttMock->receivedSize);
        }
    }

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, whenAubCommandStreamReceiverIsCreatedThenPPGTTAndGGTTCreatedHavePhysicalAddressAllocatorSet) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", false, *pDevice->executionEnvironment);
    ASSERT_NE(nullptr, aubCsr->ppgtt.get());
    ASSERT_NE(nullptr, aubCsr->ggtt.get());

    uintptr_t address = 0x20000;
    auto physicalAddress = aubCsr->ppgtt->map(address, MemoryConstants::pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(0u, physicalAddress);

    physicalAddress = aubCsr->ggtt->map(address, MemoryConstants::pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(0u, physicalAddress);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenEngineIsInitializedThenDumpHandleIsGenerated) {
    auto engineInstance = HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances()[0];
    MockOsContext osContext(0, 1, engineInstance, PreemptionMode::Disabled, false);
    MockExecutionEnvironment executionEnvironment(platformDevices[0]);

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>("", true, executionEnvironment);
    EXPECT_NE(nullptr, aubCsr);
    EXPECT_EQ(nullptr, aubCsr->hardwareContextController.get());
    aubCsr->aubManager = nullptr;

    aubCsr->setupContext(osContext);
    aubCsr->initializeEngine();
    EXPECT_NE(0u, aubCsr->handle);
}

using InjectMmmioTest = Test<DeviceFixture>;

HWTEST_F(InjectMmmioTest, givenAddMmioKeySetToZeroWhenInitAdditionalMmioCalledThenDoNotWriteMmio) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AubDumpAddMmioRegistersList.set("");

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>("", true, *pDevice->executionEnvironment);
    EXPECT_NE(nullptr, aubCsr);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr->stream = stream.get();
    EXPECT_EQ(0u, stream->mmioList.size());
    aubCsr->initAdditionalMMIO();
    EXPECT_EQ(0u, stream->mmioList.size());
}

HWTEST_F(InjectMmmioTest, givenAddMmioRegistersListSetWhenInitAdditionalMmioCalledThenWriteGivenMmio) {
    std::string registers("0xdead;0xbeef;and another very long string");
    MMIOPair mmioPair(0xdead, 0xbeef);

    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AubDumpAddMmioRegistersList.set(registers);

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>("", true, *pDevice->executionEnvironment);
    EXPECT_NE(nullptr, aubCsr);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr->stream = stream.get();
    EXPECT_EQ(0u, stream->mmioList.size());
    aubCsr->initAdditionalMMIO();
    EXPECT_EQ(1u, stream->mmioList.size());
    EXPECT_TRUE(stream->isOnMmioList(mmioPair));
};

HWTEST_F(InjectMmmioTest, givenLongSequenceOfAddMmioRegistersListSetWhenInitAdditionalMmioCalledThenWriteGivenMmio) {
    std::string registers("1;1;2;2;3;3");

    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AubDumpAddMmioRegistersList.set(registers);

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>("", true, *pDevice->executionEnvironment);
    EXPECT_NE(nullptr, aubCsr);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr->stream = stream.get();
    EXPECT_EQ(0u, stream->mmioList.size());
    aubCsr->initAdditionalMMIO();
    EXPECT_EQ(3u, stream->mmioList.size());
}

HWTEST_F(InjectMmmioTest, givenSequenceWithIncompletePairOfAddMmioRegistersListSetWhenInitAdditionalMmioCalledThenWriteGivenMmio) {
    std::string registers("0x1;0x1;0x2");
    MMIOPair mmioPair0(0x1, 0x1);
    MMIOPair mmioPair1(0x2, 0x2);

    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AubDumpAddMmioRegistersList.set(registers);

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>("", true, *pDevice->executionEnvironment);
    EXPECT_NE(nullptr, aubCsr);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr->stream = stream.get();
    EXPECT_EQ(0u, stream->mmioList.size());
    aubCsr->initAdditionalMMIO();
    EXPECT_EQ(1u, stream->mmioList.size());
    EXPECT_TRUE(stream->isOnMmioList(mmioPair0));
    EXPECT_FALSE(stream->isOnMmioList(mmioPair1));
}

HWTEST_F(InjectMmmioTest, givenAddMmioRegistersListSetWithSemicolonAtTheEndWhenInitAdditionalMmioCalledThenWriteGivenMmio) {
    std::string registers("0xdead;0xbeef;");
    MMIOPair mmioPair(0xdead, 0xbeef);

    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AubDumpAddMmioRegistersList.set(registers);

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>("", true, *pDevice->executionEnvironment);
    EXPECT_NE(nullptr, aubCsr);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr->stream = stream.get();
    EXPECT_EQ(0u, stream->mmioList.size());
    aubCsr->initAdditionalMMIO();
    EXPECT_EQ(1u, stream->mmioList.size());
    EXPECT_TRUE(stream->isOnMmioList(mmioPair));
}

HWTEST_F(InjectMmmioTest, givenAddMmioRegistersListSetWithInvalidValueWhenInitAdditionalMmioCalledThenMmioIsNotWritten) {
    std::string registers("0xdead;invalid");

    DebugManagerStateRestore stateRestore;
    DebugManager.flags.AubDumpAddMmioRegistersList.set(registers);

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>("", true, *pDevice->executionEnvironment);
    EXPECT_NE(nullptr, aubCsr);

    auto stream = std::make_unique<MockAubFileStreamMockMmioWrite>();
    aubCsr->stream = stream.get();
    EXPECT_EQ(0u, stream->mmioList.size());
    aubCsr->initAdditionalMMIO();
    EXPECT_EQ(0u, stream->mmioList.size());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCsrWhenAskedForMemoryExpectationThenPassValidCompareOperationType) {
    class MyMockAubCsr : public AUBCommandStreamReceiverHw<FamilyType> {
      public:
        using AUBCommandStreamReceiverHw<FamilyType>::AUBCommandStreamReceiverHw;

        cl_int expectMemory(const void *gfxAddress, const void *srcAddress, size_t length, uint32_t compareOperation) override {
            inputCompareOperation = compareOperation;
            AUBCommandStreamReceiverHw<FamilyType>::expectMemory(gfxAddress, srcAddress, length, compareOperation);
            return CL_SUCCESS;
        }
        uint32_t inputCompareOperation = 0;
    };
    void *mockAddress = reinterpret_cast<void *>(1);
    uint32_t compareNotEqual = AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareNotEqual;
    uint32_t compareEqual = AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual;

    auto mockStream = std::make_unique<MockAubFileStream>();
    MyMockAubCsr myMockCsr(std::string(), true, *pDevice->getExecutionEnvironment());
    myMockCsr.setupContext(pDevice->getExecutionEnvironment()->commandStreamReceivers[0][0]->getOsContext());
    myMockCsr.stream = mockStream.get();

    myMockCsr.expectMemoryNotEqual(mockAddress, mockAddress, 1);
    EXPECT_EQ(compareNotEqual, myMockCsr.inputCompareOperation);
    EXPECT_EQ(compareNotEqual, mockStream->compareOperationFromExpectMemory);

    myMockCsr.expectMemoryEqual(mockAddress, mockAddress, 1);
    EXPECT_EQ(compareEqual, myMockCsr.inputCompareOperation);
    EXPECT_EQ(compareEqual, mockStream->compareOperationFromExpectMemory);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenObtainingPreferredTagPoolSizeThenReturnOne) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, *pDevice->executionEnvironment);
    EXPECT_EQ(1u, aubCsr->getPreferredTagPoolSize());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenSshSizeIsObtainedItEqualsTo64KB) {
    auto aubCsr = std::make_unique<MockAubCsr<FamilyType>>("", true, *pDevice->executionEnvironment);
    EXPECT_EQ(64 * KB, aubCsr->defaultSshSize);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenPhysicalAddressAllocatorIsCreatedThenItIsNotNull) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment);
    std::unique_ptr<PhysicalAddressAllocator> allocator(aubCsr.createPhysicalAddressAllocator(&hardwareInfo));
    ASSERT_NE(nullptr, allocator);
}

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationWritableWhenDumpAllocationIsCalledAndDumpFormatIsSpecifiedThenGraphicsAllocationShouldBeDumped) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment);
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    aubCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    aubCsr.dumpAllocation(*gfxAllocation);

    EXPECT_TRUE(mockHardwareContext->dumpSurfaceCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenCompressedGraphicsAllocationWritableWhenDumpAllocationIsCalledAndDumpFormatIsSpecifiedThenGraphicsAllocationShouldBeDumped) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpBufferFormat.set("TRE");

    MockAubCenter *mockAubCenter = new MockAubCenter(platformDevices[0], false, "aubfile", CommandStreamReceiverType::CSR_AUB);
    mockAubCenter->aubManager = std::make_unique<MockAubManager>();

    pDevice->executionEnvironment->aubCenter.reset(mockAubCenter);
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment);
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    aubCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER_COMPRESSED});
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    aubCsr.dumpAllocation(*gfxAllocation);

    EXPECT_TRUE(mockHardwareContext->dumpSurfaceCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationWritableWhenDumpAllocationIsCalledButDumpFormatIsNotSpecifiedThenGraphicsAllocationShouldNotBeDumped) {
    DebugManagerStateRestore dbgRestore;

    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment);
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    aubCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});

    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    aubCsr.dumpAllocation(*gfxAllocation);

    EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationNonWritableWhenDumpAllocationIsCalledAndFormatIsSpecifiedThenGraphicsAllocationShouldNotBeDumped) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment);
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    aubCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});

    gfxAllocation->setMemObjectsAllocationWithWritableFlags(false);
    EXPECT_FALSE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    aubCsr.dumpAllocation(*gfxAllocation);

    EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationNotDumpableWhenDumpAllocationIsCalledAndAUBDumpAllocsOnEnqueueReadOnlyIsSetThenGraphicsAllocationShouldNotBeDumpedAndRemainNonDumpable) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpAllocsOnEnqueueReadOnly.set(true);
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment);
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    aubCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});

    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    gfxAllocation->setAllocDumpable(false);

    aubCsr.dumpAllocation(*gfxAllocation);

    EXPECT_FALSE(gfxAllocation->isAllocDumpable());
    EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationDumpableWhenDumpAllocationIsCalledAndAUBDumpAllocsOnEnqueueReadOnlyIsOnThenGraphicsAllocationShouldBeDumpedAndMarkedNonDumpable) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpAllocsOnEnqueueReadOnly.set(true);
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment);
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    aubCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});

    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    gfxAllocation->setAllocDumpable(true);

    aubCsr.dumpAllocation(*gfxAllocation);

    EXPECT_FALSE(gfxAllocation->isAllocDumpable());
    EXPECT_TRUE(mockHardwareContext->dumpSurfaceCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationWritableWhenDumpAllocationIsCalledAndDumpFormatIsSpecifiedThenPollForCompletionShouldBeCalledBeforeGraphicsAllocationIsDumped) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment);
    MockOsContext osContext(0, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    aubCsr.setupContext(osContext);
    aubCsr.latestSentTaskCount = 1;

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    aubCsr.dumpAllocation(*gfxAllocation);

    EXPECT_TRUE(mockHardwareContext->pollForCompletionCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

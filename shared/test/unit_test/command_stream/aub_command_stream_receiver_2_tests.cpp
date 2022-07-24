/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/hardware_context_controller.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/fixtures/aub_command_stream_receiver_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_aub_file_stream.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_aub_subcapture_manager.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/mock_aub_center_fixture.h"

#include "gtest/gtest.h"
#include "third_party/aub_stream/headers/aubstream.h"

using namespace NEO;

using AubCommandStreamReceiverTests = Test<AubCommandStreamReceiverFixture>;

struct FlatBatchBufferHelperAubTests : AubCommandStreamReceiverTests {
    void SetUp() override {
        DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
        AubCommandStreamReceiverTests::SetUp();
    }

    DebugManagerStateRestore restore;
};

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferFlatteningInImmediateDispatchModeThenNewCombinedBatchBufferIsCreated) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    std::unique_ptr<MemoryManager> memoryManager(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));
    auto flatBatchBufferHelper = new FlatBatchBufferHelperHw<FamilyType>(*pDevice->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(flatBatchBufferHelper);

    auto chainedBatchBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), 128u});
    auto otherAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), 128u});
    ASSERT_NE(nullptr, chainedBatchBuffer);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), 128u});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, chainedBatchBuffer, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(aubCsr->getRootDeviceIndex(), batchBuffer, sizeBatchBuffer, DispatchMode::ImmediateDispatch, pDevice->getDeviceBitfield()),
        [&](GraphicsAllocation *ptr) { memoryManager->freeGraphicsMemory(ptr); });
    EXPECT_NE(nullptr, flatBatchBuffer->getUnderlyingBuffer());
    EXPECT_EQ(alignUp(128u + 128u, MemoryConstants::pageSize), sizeBatchBuffer);

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(chainedBatchBuffer);
    memoryManager->freeGraphicsMemory(otherAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferInImmediateDispatchModeAndNoChainedBatchBufferThenCombinedBatchBufferIsNotCreated) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    std::unique_ptr<MemoryManager> memoryManager(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));
    auto flatBatchBufferHelper = new FlatBatchBufferHelperHw<FamilyType>(*pDevice->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(flatBatchBufferHelper);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(aubCsr->getRootDeviceIndex(), batchBuffer, sizeBatchBuffer, DispatchMode::ImmediateDispatch, pDevice->getDeviceBitfield()),
        [&](GraphicsAllocation *ptr) { memoryManager->freeGraphicsMemory(ptr); });
    EXPECT_EQ(nullptr, flatBatchBuffer.get());
    EXPECT_EQ(0xffffu, sizeBatchBuffer);

    memoryManager->freeGraphicsMemory(commandBuffer);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedBatchBufferAndNotImmediateOrBatchedDispatchModeThenCombinedBatchBufferIsNotCreated) {
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    std::unique_ptr<MemoryManager> memoryManager(new OsAgnosticMemoryManager(*pDevice->executionEnvironment));
    auto flatBatchBufferHelper = new FlatBatchBufferHelperHw<FamilyType>(*pDevice->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(flatBatchBufferHelper);

    auto chainedBatchBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto otherAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, chainedBatchBuffer);

    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, chainedBatchBuffer, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(aubCsr->getRootDeviceIndex(), batchBuffer, sizeBatchBuffer, DispatchMode::AdaptiveDispatch, pDevice->getDeviceBitfield()),
        [&](GraphicsAllocation *ptr) { memoryManager->freeGraphicsMemory(ptr); });
    EXPECT_EQ(nullptr, flatBatchBuffer.get());
    EXPECT_EQ(0xffffu, sizeBatchBuffer);

    memoryManager->freeGraphicsMemory(commandBuffer);
    memoryManager->freeGraphicsMemory(chainedBatchBuffer);
    memoryManager->freeGraphicsMemory(otherAllocation);
}

HWTEST_F(FlatBatchBufferHelperAubTests, givenAubCommandStreamReceiverWhenRegisterCommandChunkIsCalledThenNewChunkIsAddedToTheList) {
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(batchBuffer, sizeof(MI_BATCH_BUFFER_START));
    ASSERT_EQ(1u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList().size());
    EXPECT_EQ(128u + sizeof(MI_BATCH_BUFFER_START), aubCsr->getFlatBatchBufferHelper().getCommandChunkList()[0].endOffset);

    CommandChunk chunk;
    chunk.endOffset = 0x123;
    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(chunk);

    ASSERT_EQ(2u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList().size());
    EXPECT_EQ(0x123u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList()[1].endOffset);
}

HWTEST_F(FlatBatchBufferHelperAubTests, givenAubCommandStreamReceiverWhenRemovePatchInfoDataIsCalledThenElementIsRemovedFromPatchInfoList) {
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

HWTEST_F(FlatBatchBufferHelperAubTests, givenAubCommandStreamReceiverWhenAddGucStartMessageIsCalledThenBatchBufferAddressIsStoredInPatchInfoCollection) {
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

HWTEST_F(FlatBatchBufferHelperAubTests, givenAubCommandStreamReceiverWhenForcedBatchBufferFlatteningInBatchedDispatchModeThenNewCombinedBatchBufferIsCreated) {
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

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    size_t sizeBatchBuffer = 0u;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(aubCsr->getRootDeviceIndex(), batchBuffer, sizeBatchBuffer, DispatchMode::BatchedDispatch, pDevice->getDeviceBitfield()),
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

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    ResidencyContainer allocationsForResidency = {};

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_EQ(0u, mockHelper->flattenBatchBufferCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, givenNoCpuPtrAndNotLockableAllocationWhenGettingParametersForWriteThenLockResourceIsNotCalled) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    auto mockMemoryManager = new MockMemoryManager();

    auto memoryManagerBackup = aubExecutionEnvironment->executionEnvironment->memoryManager.release();
    aubExecutionEnvironment->executionEnvironment->memoryManager.reset(mockMemoryManager);

    EXPECT_EQ(0u, mockMemoryManager->lockResourceCalled);

    constexpr uint64_t initGpuAddress = 1234;
    constexpr size_t initSize = 10;
    MockGraphicsAllocation allocation(nullptr, initGpuAddress, initSize);
    allocation.setAllocationType(AllocationType::BUFFER);
    allocation.overrideMemoryPool(MemoryPool::LocalMemory);

    aubExecutionEnvironment->executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    MockGmm mockGmm(aubExecutionEnvironment->executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper(), nullptr, initSize, initSize, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    mockGmm.resourceParams.Flags.Info.NotLockable = true;
    allocation.setDefaultGmm(&mockGmm);

    uint64_t gpuAddress{};
    void *cpuAddress{};
    size_t size{};

    aubCsr->getParametersForWriteMemory(allocation, gpuAddress, cpuAddress, size);

    EXPECT_EQ(nullptr, cpuAddress);
    EXPECT_EQ(initGpuAddress, gpuAddress);
    EXPECT_EQ(initSize, size);

    EXPECT_EQ(0u, mockMemoryManager->lockResourceCalled);
    aubExecutionEnvironment->executionEnvironment->memoryManager.reset(memoryManagerBackup);
}

HWTEST_F(AubCommandStreamReceiverTests, givenNoCpuPtrAndLockableAllocationWhenGettingParametersForWriteThenLockResourceIsCalled) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    auto mockMemoryManager = new MockMemoryManager();

    auto memoryManagerBackup = aubExecutionEnvironment->executionEnvironment->memoryManager.release();
    aubExecutionEnvironment->executionEnvironment->memoryManager.reset(mockMemoryManager);

    EXPECT_EQ(0u, mockMemoryManager->lockResourceCalled);

    constexpr uint64_t initGpuAddress = 1234;
    constexpr size_t initSize = 10;
    MockGraphicsAllocation allocation(nullptr, initGpuAddress, initSize);
    allocation.setAllocationType(AllocationType::BUFFER_HOST_MEMORY);
    allocation.overrideMemoryPool(MemoryPool::LocalMemory);

    aubExecutionEnvironment->executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    MockGmm mockGmm(aubExecutionEnvironment->executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper(), nullptr, initSize, initSize, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    mockGmm.resourceParams.Flags.Info.NotLockable = false;
    allocation.setDefaultGmm(&mockGmm);

    uint64_t gpuAddress{};
    void *cpuAddress{};
    size_t size{};

    aubCsr->getParametersForWriteMemory(allocation, gpuAddress, cpuAddress, size);

    EXPECT_EQ(nullptr, cpuAddress);
    EXPECT_EQ(initGpuAddress, gpuAddress);
    EXPECT_EQ(initSize, size);

    EXPECT_EQ(1u, mockMemoryManager->lockResourceCalled);
    aubExecutionEnvironment->executionEnvironment->memoryManager.reset(memoryManagerBackup);
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

    auto chainedBatchBuffer = aubExecutionEnvironment->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, chainedBatchBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, chainedBatchBuffer, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    aubCsr->makeResident(*chainedBatchBuffer);

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> ptr(
        aubExecutionEnvironment->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize}),
        [&](GraphicsAllocation *ptr) { aubExecutionEnvironment->executionEnvironment->memoryManager->freeGraphicsMemory(ptr); });

    auto expectedAllocation = ptr.get();
    mockHelper->flattenBatchBufferResult = ptr.release();

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_EQ(batchBuffer.commandBufferAllocation, expectedAllocation);

    aubExecutionEnvironment->executionEnvironment->memoryManager->freeGraphicsMemory(chainedBatchBuffer);

    EXPECT_EQ(1u, mockHelper->flattenBatchBufferCalled);
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
    mockHelper->flattenBatchBufferParamsPassed.clear();

    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_EQ(1u, mockHelper->flattenBatchBufferCalled);
    EXPECT_EQ(aubCsr->getRootDeviceIndex(), mockHelper->flattenBatchBufferParamsPassed[0].rootDeviceIndex);
    EXPECT_EQ(aubCsr->getOsContext().getDeviceBitfield().to_ulong(), mockHelper->flattenBatchBufferParamsPassed[0].deviceBitfield.to_ulong());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedFlattenBatchBufferAndBatchedDispatchModeThenExpectFlattenBatchBufferIsCalledAnyway) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::BatchedDispatch));

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*aubExecutionEnvironment->executionEnvironment);
    mockHelper->flattenBatchBufferParamsPassed.clear();
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);
    ResidencyContainer allocationsForResidency;

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 128u, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_EQ(1u, mockHelper->flattenBatchBufferCalled);
    EXPECT_EQ(aubCsr->getRootDeviceIndex(), mockHelper->flattenBatchBufferParamsPassed[0].rootDeviceIndex);
    EXPECT_EQ(aubCsr->getOsContext().getDeviceBitfield().to_ulong(), mockHelper->flattenBatchBufferParamsPassed[0].deviceBitfield.to_ulong());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAddPatchInfoCommentsForAUBDumpIsSetThenAddPatchInfoCommentsIsCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    ResidencyContainer allocationsForResidency;

    aubCsr->flush(batchBuffer, allocationsForResidency);
    EXPECT_EQ(1u, aubCsr->addPatchInfoCommentsCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenAddPatchInfoCommentsForAUBDumpIsNotSetThenAddPatchInfoCommentsIsNotCalled) {
    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    ResidencyContainer allocationsForResidency;

    aubCsr->flush(batchBuffer, allocationsForResidency);
    EXPECT_EQ(0u, aubCsr->addPatchInfoCommentsCalled);
}

HWTEST_F(FlatBatchBufferHelperAubTests, givenAubCommandStreamReceiverWhenGetIndirectPatchCommandsIsCalledForEmptyPatchInfoListThenIndirectPatchCommandBufferIsNotCreated) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, false, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();

    size_t indirectPatchCommandsSize = 0u;
    std::vector<PatchInfoData> indirectPatchInfo;

    std::unique_ptr<char> commandBuffer(aubCsr->getFlatBatchBufferHelper().getIndirectPatchCommands(indirectPatchCommandsSize, indirectPatchInfo));
    EXPECT_EQ(0u, indirectPatchCommandsSize);
    EXPECT_EQ(0u, indirectPatchInfo.size());
}

HWTEST_F(FlatBatchBufferHelperAubTests, givenAubCommandStreamReceiverWhenGetIndirectPatchCommandsIsCalledForNonEmptyPatchInfoListThenIndirectPatchCommandBufferIsCreated) {
    typedef typename FamilyType::MI_STORE_DATA_IMM MI_STORE_DATA_IMM;
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

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

HWTEST_F(FlatBatchBufferHelperAubTests, GivenAubCommandStreamReceiverWhenGetIndirectPatchCommandsIsCalledFor64BitAddressingModeThenDwordLengthAndStoreQwordAreSetCorrectly) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    PatchInfoData patchInfo(0xA000, 0u, PatchInfoAllocationType::KernelArg, 0x6000, 0x100, PatchInfoAllocationType::IndirectObjectHeap, sizeof(uint64_t));
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo);

    size_t indirectPatchCommandsSize = 0u;
    std::vector<PatchInfoData> indirectPatchInfo;

    std::unique_ptr<char> commandBuffer(aubCsr->getFlatBatchBufferHelper().getIndirectPatchCommands(indirectPatchCommandsSize, indirectPatchInfo));
    ASSERT_EQ(sizeof(MI_STORE_DATA_IMM), indirectPatchCommandsSize);
    ASSERT_EQ(2u, indirectPatchInfo.size());

    auto cmd = reinterpret_cast<MI_STORE_DATA_IMM *>(commandBuffer.get());
    EXPECT_TRUE(cmd->getStoreQword());
    EXPECT_EQ(MI_STORE_DATA_IMM::DWORD_LENGTH::DWORD_LENGTH_STORE_QWORD, cmd->getDwordLength());
}

HWTEST_F(FlatBatchBufferHelperAubTests, GivenAubCommandStreamReceiverWhenGetIndirectPatchCommandsIsCalledFor32BitAddressingModeThenDwordLengthAndSetStoreDwordAreSetCorrectly) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    PatchInfoData patchInfo(0xA000, 0u, PatchInfoAllocationType::KernelArg, 0x6000, 0x100, PatchInfoAllocationType::IndirectObjectHeap, sizeof(uint32_t));
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo);

    size_t indirectPatchCommandsSize = 0u;
    std::vector<PatchInfoData> indirectPatchInfo;

    std::unique_ptr<char> commandBuffer(aubCsr->getFlatBatchBufferHelper().getIndirectPatchCommands(indirectPatchCommandsSize, indirectPatchInfo));
    ASSERT_EQ(sizeof(MI_STORE_DATA_IMM), indirectPatchCommandsSize);
    ASSERT_EQ(2u, indirectPatchInfo.size());

    auto cmd = reinterpret_cast<MI_STORE_DATA_IMM *>(commandBuffer.get());
    EXPECT_FALSE(cmd->getStoreQword());
    EXPECT_EQ(MI_STORE_DATA_IMM::DWORD_LENGTH::DWORD_LENGTH_STORE_DWORD, cmd->getDwordLength());
}

HWTEST_F(FlatBatchBufferHelperAubTests, givenAubCommandStreamReceiverWhenAddBatchBufferStartCalledAndBatchBUfferFlatteningEnabledThenBatchBufferStartAddressIsRegistered) {
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

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
        auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
        auto canonizedGpuAddress = gmmHelper->canonize(imageAllocation->getGpuAddress());

        cpuPtr = imageAllocation->getUnderlyingBuffer();
        imageAllocation->setCpuPtrAndGpuAddress(nullptr, canonizedGpuAddress);
        return imageAllocation;
    };
    void freeGraphicsMemoryImpl(GraphicsAllocation *imageAllocation) override {
        auto gmmHelper = getGmmHelper(imageAllocation->getRootDeviceIndex());
        auto canonizedGpuAddress = gmmHelper->canonize(imageAllocation->getGpuAddress());
        imageAllocation->setCpuPtrAndGpuAddress(cpuPtr, canonizedGpuAddress);

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
    ExecutionEnvironment *executionEnvironment = new MockExecutionEnvironment();
    auto memoryManager = new OsAgnosticMemoryManagerForImagesWithNoHostPtr(*executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    auto engineInstance = HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[0];
    UltDeviceFactory deviceFactory{1, 0, *executionEnvironment};
    DeviceBitfield deviceBitfield(1);
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(engineInstance, deviceBitfield));
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *executionEnvironment, 0, deviceBitfield));
    aubCsr->setupContext(osContext);
    aubCsr->initializeEngine();

    ImageDescriptor imgDesc = {};
    imgDesc.imageWidth = 512;
    imgDesc.imageHeight = 1;
    imgDesc.imageType = ImageType::Image2D;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    AllocationProperties allocProperties{0u /* rootDeviceIndex */, true /* allocateMemory */,
                                         imgInfo, AllocationType::IMAGE, deviceBitfield};

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
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto memoryManager = new OsAgnosticMemoryManagerForImagesWithNoHostPtr(*executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    DeviceBitfield deviceBitfield(1);
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *executionEnvironment, 0, deviceBitfield));
    auto osContext = memoryManager->createAndRegisterOsContext(aubCsr.get(), EngineDescriptorHelper::getDefaultDescriptor({getChosenEngineType(*defaultHwInfo), EngineUsage::Regular}));
    aubCsr->setupContext(*osContext);
    aubCsr->initializeEngine();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{aubCsr->getRootDeviceIndex(), MemoryConstants::pageSize});

    memoryManager->lockResource(gfxAllocation);
    EXPECT_TRUE(aubCsr->writeMemory(*gfxAllocation));

    EXPECT_FALSE(memoryManager->unlockResourceParam.wasCalled);
    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenNoDbgDeviceIdFlagWhenAubCsrIsCreatedThenUseDefaultDeviceId) {
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    EXPECT_EQ(pDevice->getHardwareInfo().capabilityTable.aubDeviceId, aubCsr->aubDeviceId);
}

HWTEST_F(AubCommandStreamReceiverTests, givenDbgDeviceIdFlagIsSetWhenAubCsrIsCreatedThenUseDebugDeviceId) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.OverrideAubDeviceId.set(9); // this is Hsw, not used
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    EXPECT_EQ(9u, aubCsr->aubDeviceId);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenGetGTTDataIsCalledThenLocalMemoryIsSetAccordingToCsrFeature) {
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
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
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    aubCsr->localMemoryEnabled = false;

    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    auto bank = aubCsr->getMemoryBankForGtt();
    EXPECT_EQ(MemoryBanks::MainBank, bank);
}

HWTEST_F(AubCommandStreamReceiverTests, givenEntryBitsPresentAndWritableWhenGetAddressSpaceFromPTEBitsIsCalledThenTraceNonLocalIsReturned) {
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

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
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
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
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
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
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
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
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    size_t size = 100;
    auto ptr = std::make_unique<char[]>(size);
    auto addr = reinterpret_cast<uint64_t>(ptr.get());
    AllocationView externalAllocation(addr, size);
    aubCsr->makeResidentExternal(externalAllocation);

    ASSERT_EQ(1u, aubCsr->externalAllocations.size());
    ResidencyContainer allocationsForResidency;
    aubCsr->processResidency(allocationsForResidency, 0u);

    EXPECT_TRUE(aubCsr->writeMemoryParametrization.wasCalled);
    EXPECT_EQ(addr, aubCsr->writeMemoryParametrization.receivedAllocationView.first);
    EXPECT_EQ(size, aubCsr->writeMemoryParametrization.receivedAllocationView.second);
    EXPECT_TRUE(aubCsr->writeMemoryParametrization.statusToReturn);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenProcessResidencyIsCalledThenExternalAllocationWithZeroSizeShouldNotBeMadeResident) {
    auto aubCsr = std::make_unique<MockAubCsrToTestExternalAllocations<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    AllocationView externalAllocation(0, 0);
    aubCsr->makeResidentExternal(externalAllocation);

    ASSERT_EQ(1u, aubCsr->externalAllocations.size());
    ResidencyContainer allocationsForResidency;
    aubCsr->processResidency(allocationsForResidency, 0u);

    EXPECT_TRUE(aubCsr->writeMemoryParametrization.wasCalled);
    EXPECT_EQ(0u, aubCsr->writeMemoryParametrization.receivedAllocationView.first);
    EXPECT_EQ(0u, aubCsr->writeMemoryParametrization.receivedAllocationView.second);
    EXPECT_FALSE(aubCsr->writeMemoryParametrization.statusToReturn);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenWriteMemoryIsCalledThenGraphicsAllocationSizeIsReadCorrectly) {
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter.reset(new AubCenter());

    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    aubCsr->initializeEngine();
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

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    aubCsr->setAubWritable(true, *gfxAllocation);

    auto gmm = new Gmm(pDevice->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    gfxAllocation->setDefaultGmm(gmm);

    for (bool compressed : {false, true}) {
        gmm->isCompressionEnabled = compressed;

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
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    ASSERT_NE(nullptr, aubCsr->ppgtt.get());
    ASSERT_NE(nullptr, aubCsr->ggtt.get());

    uintptr_t address = 0x20000;
    auto physicalAddress = aubCsr->ppgtt->map(address, MemoryConstants::pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(0u, physicalAddress);

    physicalAddress = aubCsr->ggtt->map(address, MemoryConstants::pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(0u, physicalAddress);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenEngineIsInitializedThenDumpHandleIsGenerated) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
    auto engineInstance = HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[0];
    DeviceBitfield deviceBitfield(1);
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(engineInstance, deviceBitfield));
    executionEnvironment.initializeMemoryManager();

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>("", true, executionEnvironment, 0, deviceBitfield);
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

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
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

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
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

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
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

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
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

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
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

    auto aubCsr = std::make_unique<MockAubCsrToTestDumpContext<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
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

        bool expectMemory(const void *gfxAddress, const void *srcAddress, size_t length, uint32_t compareOperation) override {
            inputCompareOperation = compareOperation;
            return AUBCommandStreamReceiverHw<FamilyType>::expectMemory(gfxAddress, srcAddress, length, compareOperation);
        }
        uint32_t inputCompareOperation = 0;
    };
    void *mockAddress = reinterpret_cast<void *>(1);
    uint32_t compareNotEqual = AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareNotEqual;
    uint32_t compareEqual = AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual;

    auto mockStream = std::make_unique<MockAubFileStream>();
    MyMockAubCsr myMockCsr(std::string(), true, *pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    myMockCsr.setupContext(pDevice->commandStreamReceivers[0]->getOsContext());
    myMockCsr.stream = mockStream.get();

    myMockCsr.expectMemoryNotEqual(mockAddress, mockAddress, 1);
    EXPECT_EQ(compareNotEqual, myMockCsr.inputCompareOperation);
    EXPECT_EQ(compareNotEqual, mockStream->compareOperationFromExpectMemory);

    myMockCsr.expectMemoryEqual(mockAddress, mockAddress, 1);
    EXPECT_EQ(compareEqual, myMockCsr.inputCompareOperation);
    EXPECT_EQ(compareEqual, mockStream->compareOperationFromExpectMemory);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenObtainingPreferredTagPoolSizeThenReturnOne) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    EXPECT_EQ(1u, aubCsr->getPreferredTagPoolSize());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenSshSizeIsObtainedThenReturn64KB) {
    auto aubCsr = std::make_unique<MockAubCsr<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    EXPECT_EQ(64 * KB, aubCsr->defaultSshSize);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenPhysicalAddressAllocatorIsCreatedThenItIsNotNull) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::unique_ptr<PhysicalAddressAllocator> allocator(aubCsr.createPhysicalAddressAllocator(&hardwareInfo));
    ASSERT_NE(nullptr, allocator);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWithoutHardwareContextControllerWhenCallingWriteMMIOThenDontRedirectToHardwareContextController) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    EXPECT_EQ(nullptr, aubCsr.hardwareContextController);

    aubCsr.writeMMIO(0x11111111, 0x22222222);

    EXPECT_TRUE(aubCsr.writeMMIOCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWithHardwareContextControllerWhenCallingWriteMMIOThenRedirectToHardwareContextController) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    EXPECT_NE(nullptr, aubCsr.hardwareContextController);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    aubCsr.writeMMIO(0x11111111, 0x22222222);

    EXPECT_TRUE(mockHardwareContext->writeMMIOCalled);
}

/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hardware_context_controller.h"
#include "shared/source/memory_manager/page_table.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/fixtures/aub_command_stream_receiver_fixture.h"
#include "shared/test/common/fixtures/mock_aub_center_fixture.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_aub_subcapture_manager.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "aubstream/aubstream.h"
#include "gtest/gtest.h"

using namespace NEO;

using AubCommandStreamReceiverTests = Test<AubCommandStreamReceiverFixture>;
using AubCommandStreamReceiverWithoutAubStreamTests = Test<AubCommandStreamReceiverWithoutAubStreamFixture>;

struct FlatBatchBufferHelperAubTests : AubCommandStreamReceiverTests {
    void SetUp() override {
        debugManager.flags.FlattenBatchBufferForAUBDump.set(true);
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

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed(), 128);
    batchBuffer.chainedBatchBuffer = chainedBatchBuffer;
    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(aubCsr->getRootDeviceIndex(), batchBuffer, sizeBatchBuffer, DispatchMode::immediateDispatch, pDevice->getDeviceBitfield()),
        [&](GraphicsAllocation *ptr) { memoryManager->freeGraphicsMemory(ptr); });
    EXPECT_NE(nullptr, flatBatchBuffer->getUnderlyingBuffer());
    size_t expectedAlignedSize = alignUp(128u, MemoryConstants::pageSize) + alignUp(128u, MemoryConstants::pageSize);
    EXPECT_EQ(expectedAlignedSize, sizeBatchBuffer);

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

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed(), 128);

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(aubCsr->getRootDeviceIndex(), batchBuffer, sizeBatchBuffer, DispatchMode::immediateDispatch, pDevice->getDeviceBitfield()),
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

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed(), 128);

    size_t sizeBatchBuffer = 0xffffu;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(aubCsr->getRootDeviceIndex(), batchBuffer, sizeBatchBuffer, DispatchMode::adaptiveDispatch, pDevice->getDeviceBitfield()),
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

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed(), 128);

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

    PatchInfoData patchInfoData(0xA000, 0x0, PatchInfoAllocationType::kernelArg, 0xB000, 0x0, PatchInfoAllocationType::defaultType);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData);
    EXPECT_EQ(1u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());

    EXPECT_TRUE(aubCsr->getFlatBatchBufferHelper().removePatchInfoData(0xC000));
    EXPECT_EQ(1u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());

    EXPECT_TRUE(aubCsr->getFlatBatchBufferHelper().removePatchInfoData(0xB000));
    EXPECT_EQ(0u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());
}

HWTEST_F(FlatBatchBufferHelperAubTests, givenAubCommandStreamReceiverWhenForcedBatchBufferFlatteningInBatchedDispatchModeThenNewCombinedBatchBufferIsCreated) {
    debugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);
    debugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::batchedDispatch));

    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();
    auto memoryManager = aubExecutionEnvironment->executionEnvironment->memoryManager.get();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    CommandChunk chunk1;
    CommandChunk chunk2;
    CommandChunk chunk3;

    std::unique_ptr<char[]> commands1(new char[0x100u]);
    commands1.get()[0] = 0x1;
    chunk1.baseAddressCpu = chunk1.baseAddressGpu = reinterpret_cast<uint64_t>(commands1.get());
    chunk1.startOffset = 0u;
    chunk1.endOffset = 0x50u;

    std::unique_ptr<char[]> commands2(new char[0x100u]);
    commands2.get()[0] = 0x2;
    chunk2.baseAddressCpu = chunk2.baseAddressGpu = reinterpret_cast<uint64_t>(commands2.get());
    chunk2.startOffset = 0u;
    chunk2.endOffset = 0x50u;
    aubCsr->getFlatBatchBufferHelper().registerBatchBufferStartAddress(reinterpret_cast<uint64_t>(commands2.get() + 0x40), reinterpret_cast<uint64_t>(commands1.get()));

    std::unique_ptr<char[]> commands3(new char[0x100u]);
    commands3.get()[0] = 0x3;
    chunk3.baseAddressCpu = chunk3.baseAddressGpu = reinterpret_cast<uint64_t>(commands3.get());
    chunk3.startOffset = 0u;
    chunk3.endOffset = 0x50u;
    aubCsr->getFlatBatchBufferHelper().registerBatchBufferStartAddress(reinterpret_cast<uint64_t>(commands3.get() + 0x40), reinterpret_cast<uint64_t>(commands2.get()));

    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(chunk1);
    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(chunk2);
    aubCsr->getFlatBatchBufferHelper().registerCommandChunk(chunk3);

    ASSERT_EQ(3u, aubCsr->getFlatBatchBufferHelper().getCommandChunkList().size());

    PatchInfoData patchInfoData1(0xAAAu, 0xAu, PatchInfoAllocationType::indirectObjectHeap, chunk1.baseAddressGpu, 0x10, PatchInfoAllocationType::defaultType);
    PatchInfoData patchInfoData2(0xBBBu, 0xAu, PatchInfoAllocationType::indirectObjectHeap, chunk1.baseAddressGpu, 0x60, PatchInfoAllocationType::defaultType);
    PatchInfoData patchInfoData3(0xCCCu, 0xAu, PatchInfoAllocationType::indirectObjectHeap, 0x0, 0x10, PatchInfoAllocationType::defaultType);

    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData1);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData2);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfoData3);

    ASSERT_EQ(3u, aubCsr->getFlatBatchBufferHelper().getPatchInfoCollection().size());

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed(), 128);

    size_t sizeBatchBuffer = 0u;

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> flatBatchBuffer(
        aubCsr->getFlatBatchBufferHelper().flattenBatchBuffer(aubCsr->getRootDeviceIndex(), batchBuffer, sizeBatchBuffer, DispatchMode::batchedDispatch, pDevice->getDeviceBitfield()),
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

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
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
    allocation.setAllocationType(AllocationType::buffer);
    allocation.overrideMemoryPool(MemoryPool::localMemory);

    aubExecutionEnvironment->executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    MockGmm mockGmm(aubExecutionEnvironment->executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper(), nullptr, initSize, initSize, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    mockGmm.resourceParams.Flags.Info.NotLockable = true;
    allocation.setDefaultGmm(&mockGmm);

    uint64_t gpuAddress{};
    void *cpuAddress{};
    size_t size{};

    aubCsr->getParametersForMemory(allocation, gpuAddress, cpuAddress, size);

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
    allocation.setAllocationType(AllocationType::bufferHostMemory);
    allocation.overrideMemoryPool(MemoryPool::localMemory);

    aubExecutionEnvironment->executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    MockGmm mockGmm(aubExecutionEnvironment->executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper(), nullptr, initSize, initSize, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    mockGmm.resourceParams.Flags.Info.NotLockable = false;
    allocation.setDefaultGmm(&mockGmm);

    uint64_t gpuAddress{};
    void *cpuAddress{};
    size_t size{};

    aubCsr->getParametersForMemory(allocation, gpuAddress, cpuAddress, size);

    EXPECT_EQ(nullptr, cpuAddress);
    EXPECT_EQ(initGpuAddress, gpuAddress);
    EXPECT_EQ(initSize, size);

    EXPECT_EQ(1u, mockMemoryManager->lockResourceCalled);
    aubExecutionEnvironment->executionEnvironment->memoryManager.reset(memoryManagerBackup);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedFlattenBatchBufferAndImmediateDispatchModeThenExpectFlattenBatchBufferIsCalled) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    debugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::immediateDispatch));

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    auto allocationsForResidency = aubCsr->getResidencyAllocations();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*aubExecutionEnvironment->executionEnvironment);
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);

    auto chainedBatchBuffer = aubExecutionEnvironment->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, chainedBatchBuffer);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed(), 128);

    aubCsr->makeResident(*chainedBatchBuffer);

    std::unique_ptr<GraphicsAllocation, std::function<void(GraphicsAllocation *)>> ptr(
        aubExecutionEnvironment->executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize}),
        [&](GraphicsAllocation *ptr) { aubExecutionEnvironment->executionEnvironment->memoryManager->freeGraphicsMemory(ptr); });

    auto expectedBatchBufferGpuAddress = ptr->getGpuAddress();
    mockHelper->flattenBatchBufferResult = ptr.release();

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_EQ(aubCsr->batchBufferGpuAddressPassed, expectedBatchBufferGpuAddress);
    EXPECT_NE(aubCsr->batchBufferGpuAddressPassed, batchBuffer.commandBufferAllocation->getGpuAddress());
    EXPECT_EQ(batchBuffer.commandBufferAllocation, cs.getGraphicsAllocation());

    aubExecutionEnvironment->executionEnvironment->memoryManager->freeGraphicsMemory(chainedBatchBuffer);

    EXPECT_EQ(1u, mockHelper->flattenBatchBufferCalled);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedFlattenBatchBufferAndImmediateDispatchModeAndThereIsNoChainedBatchBufferThenExpectFlattenBatchBufferIsCalledAnyway) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    debugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::immediateDispatch));

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    auto allocationsForResidency = aubCsr->getResidencyAllocations();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*aubExecutionEnvironment->executionEnvironment);
    mockHelper->flattenBatchBufferParamsPassed.clear();

    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed(), 128);

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_EQ(1u, mockHelper->flattenBatchBufferCalled);
    EXPECT_EQ(aubCsr->getRootDeviceIndex(), mockHelper->flattenBatchBufferParamsPassed[0].rootDeviceIndex);
    EXPECT_EQ(aubCsr->getOsContext().getDeviceBitfield().to_ulong(), mockHelper->flattenBatchBufferParamsPassed[0].deviceBitfield.to_ulong());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenForcedFlattenBatchBufferAndBatchedDispatchModeThenExpectFlattenBatchBufferIsCalledAnyway) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.FlattenBatchBufferForAUBDump.set(true);
    debugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::batchedDispatch));

    auto aubExecutionEnvironment = getEnvironment<MockAubCsr<FamilyType>>(true, true, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<MockAubCsr<FamilyType>>();
    LinearStream cs(aubExecutionEnvironment->commandBuffer);

    auto mockHelper = new MockFlatBatchBufferHelper<FamilyType>(*aubExecutionEnvironment->executionEnvironment);
    mockHelper->flattenBatchBufferParamsPassed.clear();
    aubCsr->overwriteFlatBatchBufferHelper(mockHelper);
    ResidencyContainer allocationsForResidency;

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed(), 128);

    aubCsr->flush(batchBuffer, allocationsForResidency);

    EXPECT_EQ(1u, mockHelper->flattenBatchBufferCalled);
    EXPECT_EQ(aubCsr->getRootDeviceIndex(), mockHelper->flattenBatchBufferParamsPassed[0].rootDeviceIndex);
    EXPECT_EQ(aubCsr->getOsContext().getDeviceBitfield().to_ulong(), mockHelper->flattenBatchBufferParamsPassed[0].deviceBitfield.to_ulong());
}

HWTEST_F(FlatBatchBufferHelperAubTests, givenAubCommandStreamReceiverWhenGetIndirectPatchCommandsIsCalledForEmptyPatchInfoListThenIndirectPatchCommandBufferIsNotCreated) {
    auto aubExecutionEnvironment = getEnvironment<AUBCommandStreamReceiverHw<FamilyType>>(false, false, true);
    auto aubCsr = aubExecutionEnvironment->template getCsr<AUBCommandStreamReceiverHw<FamilyType>>();

    size_t indirectPatchCommandsSize = 0u;
    std::vector<PatchInfoData> indirectPatchInfo;

    std::unique_ptr<char[]> commandBuffer(aubCsr->getFlatBatchBufferHelper().getIndirectPatchCommands(indirectPatchCommandsSize, indirectPatchInfo));
    EXPECT_EQ(0u, indirectPatchCommandsSize);
    EXPECT_EQ(0u, indirectPatchInfo.size());
}

HWTEST_F(FlatBatchBufferHelperAubTests, givenAubCommandStreamReceiverWhenGetIndirectPatchCommandsIsCalledForNonEmptyPatchInfoListThenIndirectPatchCommandBufferIsCreated) {
    typedef typename FamilyType::MI_STORE_DATA_IMM MI_STORE_DATA_IMM;
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    PatchInfoData patchInfo1(0xA000, 0u, PatchInfoAllocationType::kernelArg, 0x6000, 0x100, PatchInfoAllocationType::indirectObjectHeap);
    PatchInfoData patchInfo2(0xB000, 0u, PatchInfoAllocationType::kernelArg, 0x6000, 0x200, PatchInfoAllocationType::indirectObjectHeap);
    PatchInfoData patchInfo3(0xC000, 0u, PatchInfoAllocationType::indirectObjectHeap, 0x1000, 0x100, PatchInfoAllocationType::defaultType);
    PatchInfoData patchInfo4(0xC000, 0u, PatchInfoAllocationType::defaultType, 0x2000, 0x100, PatchInfoAllocationType::gucStartMessage);

    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo1);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo2);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo3);
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo4);

    size_t indirectPatchCommandsSize = 0u;
    std::vector<PatchInfoData> indirectPatchInfo;

    std::unique_ptr<char[]> commandBuffer(aubCsr->getFlatBatchBufferHelper().getIndirectPatchCommands(indirectPatchCommandsSize, indirectPatchInfo));
    EXPECT_EQ(4u, indirectPatchInfo.size());
    EXPECT_EQ(2u * sizeof(MI_STORE_DATA_IMM), indirectPatchCommandsSize);
}

HWTEST_F(FlatBatchBufferHelperAubTests, GivenAubCommandStreamReceiverWhenGetIndirectPatchCommandsIsCalledFor64BitAddressingModeThenDwordLengthAndStoreQwordAreSetCorrectly) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    PatchInfoData patchInfo(0xA000, 0u, PatchInfoAllocationType::kernelArg, 0x6000, 0x100, PatchInfoAllocationType::indirectObjectHeap, sizeof(uint64_t));
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo);

    size_t indirectPatchCommandsSize = 0u;
    std::vector<PatchInfoData> indirectPatchInfo;

    std::unique_ptr<char[]> commandBuffer(aubCsr->getFlatBatchBufferHelper().getIndirectPatchCommands(indirectPatchCommandsSize, indirectPatchInfo));
    ASSERT_EQ(sizeof(MI_STORE_DATA_IMM), indirectPatchCommandsSize);
    ASSERT_EQ(2u, indirectPatchInfo.size());

    auto cmd = reinterpret_cast<MI_STORE_DATA_IMM *>(commandBuffer.get());
    EXPECT_TRUE(cmd->getStoreQword());
    EXPECT_EQ(MI_STORE_DATA_IMM::DWORD_LENGTH::DWORD_LENGTH_STORE_QWORD, cmd->getDwordLength());
}

HWTEST_F(FlatBatchBufferHelperAubTests, GivenAubCommandStreamReceiverWhenGetIndirectPatchCommandsIsCalledFor32BitAddressingModeThenDwordLengthAndSetStoreDwordAreSetCorrectly) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));

    PatchInfoData patchInfo(0xA000, 0u, PatchInfoAllocationType::kernelArg, 0x6000, 0x100, PatchInfoAllocationType::indirectObjectHeap, sizeof(uint32_t));
    aubCsr->getFlatBatchBufferHelper().setPatchInfoData(patchInfo);

    size_t indirectPatchCommandsSize = 0u;
    std::vector<PatchInfoData> indirectPatchInfo;

    std::unique_ptr<char[]> commandBuffer(aubCsr->getFlatBatchBufferHelper().getIndirectPatchCommands(indirectPatchCommandsSize, indirectPatchInfo));
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
HWTEST_F(AubCommandStreamReceiverNoHostPtrTests, givenAubCommandStreamReceiverWhenWriteMemoryIsCalledOnLockableImageWithNoHostPtrThenResourceShouldBeLockedToGetCpuAddress) {
    ExecutionEnvironment *executionEnvironment = new MockExecutionEnvironment();
    auto memoryManager = new OsAgnosticMemoryManagerForImagesWithNoHostPtr(*executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto engineInstance = gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment->rootDeviceEnvironments[0])[0];
    UltDeviceFactory deviceFactory{1, 0, *executionEnvironment};
    DeviceBitfield deviceBitfield(1);
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(engineInstance, deviceBitfield));
    std::unique_ptr<AUBCommandStreamReceiverHw<FamilyType>> aubCsr(new AUBCommandStreamReceiverHw<FamilyType>("", true, *executionEnvironment, 0, deviceBitfield));
    aubCsr->setupContext(osContext);
    aubCsr->initializeEngine();

    ImageDescriptor imgDesc = {};
    imgDesc.imageWidth = 512;
    imgDesc.imageHeight = 1;
    imgDesc.imageType = ImageType::image2D;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    AllocationProperties allocProperties{0u /* rootDeviceIndex */, true /* allocateMemory */, &imgInfo, AllocationType::image, deviceBitfield};
    auto imageAllocation = memoryManager->allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);
    ASSERT_NE(nullptr, imageAllocation);
    imageAllocation->getDefaultGmm()->resourceParams.Flags.Info.NotLockable = false;

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
    auto osContext = memoryManager->createAndRegisterOsContext(aubCsr.get(), EngineDescriptorHelper::getDefaultDescriptor({getChosenEngineType(*defaultHwInfo), EngineUsage::regular}));
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
    debugManager.flags.OverrideAubDeviceId.set(9); // this is Hsw, not used
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

HWTEST_F(AubCommandStreamReceiverTests, whenGetMemoryBankForGttIsCalledThenCorrectBankIsReturned) {
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield()));
    aubCsr->localMemoryEnabled = false;

    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    auto bank = aubCsr->getMemoryBankForGtt();
    EXPECT_EQ(MemoryBanks::mainBank, bank);
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

HWTEST_F(AubCommandStreamReceiverTests, whenAubCommandStreamReceiverIsCreatedThenPPGTTAndGGTTCreatedHavePhysicalAddressAllocatorSet) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    ASSERT_NE(nullptr, aubCsr->ppgtt.get());
    ASSERT_NE(nullptr, aubCsr->ggtt.get());

    uintptr_t address = 0x20000;
    auto physicalAddress = aubCsr->ppgtt->map(address, MemoryConstants::pageSize, 0, MemoryBanks::mainBank);
    EXPECT_NE(0u, physicalAddress);

    physicalAddress = aubCsr->ggtt->map(address, MemoryConstants::pageSize, 0, MemoryBanks::mainBank);
    EXPECT_NE(0u, physicalAddress);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenObtainingPreferredTagPoolSizeThenReturnOne) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    EXPECT_EQ(1u, aubCsr->getPreferredTagPoolSize());
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenSshSizeIsObtainedThenReturn64KB) {
    auto aubCsr = std::make_unique<MockAubCsr<FamilyType>>("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    EXPECT_EQ(64 * MemoryConstants::kiloByte, aubCsr->defaultSshSize);
}

HWTEST_F(AubCommandStreamReceiverTests, givenAubCommandStreamReceiverWhenPhysicalAddressAllocatorIsCreatedThenItIsNotNull) {
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    std::unique_ptr<PhysicalAddressAllocator> allocator(aubCsr.createPhysicalAddressAllocator(&hardwareInfo, pDevice->getReleaseHelper()));
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

HWTEST_F(AubCommandStreamReceiverTests, givenTimestampBufferAllocationWhenAubWriteMemoryIsCalledForAllocationThenItIsOneTimeWriteable) {
    auto aubCsr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    aubCsr->setupContext(*pDevice->getDefaultEngine().osContext);
    aubCsr->initializeEngine();

    MemoryManager *memoryManager = aubCsr->getMemoryManager();
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

    timestampAllocation->setAubWritable(true, GraphicsAllocation::defaultBank);

    EXPECT_TRUE(aubCsr->writeMemory(*timestampAllocation));
    EXPECT_FALSE(timestampAllocation->isAubWritable(GraphicsAllocation::defaultBank));

    memoryManager->freeGraphicsMemory(timestampAllocation);
}
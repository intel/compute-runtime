/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_alloc_dump.h"
#include "shared/source/helpers/hardware_context_controller.h"
#include "shared/test/common/fixtures/aub_command_stream_receiver_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using AubCsrTest = ::testing::Test;

HWTEST_F(AubCsrTest, givenLocalMemoryEnabledWhenGettingAddressSpaceForRingDataTypeThenTraceLocalIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(1);
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrLocalMemory = true;

    std::unique_ptr<ExecutionEnvironment> executionEnvironment(new ExecutionEnvironment);
    DeviceBitfield deviceBitfield(1);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    uint32_t rootDeviceIndex = 0u;

    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfo(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->aubCenter.reset(new AubCenter());

    executionEnvironment->initializeMemoryManager();

    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", false, *executionEnvironment, rootDeviceIndex, deviceBitfield));

    int types[] = {AubMemDump::DataTypeHintValues::TraceLogicalRingContextRcs,
                   AubMemDump::DataTypeHintValues::TraceLogicalRingContextCcs,
                   AubMemDump::DataTypeHintValues::TraceLogicalRingContextBcs,
                   AubMemDump::DataTypeHintValues::TraceLogicalRingContextVcs,
                   AubMemDump::DataTypeHintValues::TraceLogicalRingContextVecs,
                   AubMemDump::DataTypeHintValues::TraceCommandBuffer};

    for (uint32_t i = 0; i < 6; i++) {
        auto addressSpace = aubCsr->getAddressSpace(types[i]);
        EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceLocal, addressSpace);
    }

    auto addressSpace = aubCsr->getAddressSpace(AubMemDump::DataTypeHintValues::TraceNotype);
    EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceNonlocal, addressSpace);
}

HWTEST_F(AubCsrTest, givenAUBDumpForceAllToLocalMemoryWhenGettingAddressSpaceForAnyDataTypeThenTraceLocalIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.AUBDumpForceAllToLocalMemory.set(1);
    auto hwInfo = *NEO::defaultHwInfo.get();

    std::unique_ptr<ExecutionEnvironment> executionEnvironment(new ExecutionEnvironment);
    DeviceBitfield deviceBitfield(1);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    uint32_t rootDeviceIndex = 0u;

    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfo(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->aubCenter.reset(new AubCenter());

    executionEnvironment->initializeMemoryManager();

    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", false, *executionEnvironment, rootDeviceIndex, deviceBitfield));

    int types[] = {AubMemDump::DataTypeHintValues::TraceLogicalRingContextRcs,
                   AubMemDump::DataTypeHintValues::TraceLogicalRingContextCcs,
                   AubMemDump::DataTypeHintValues::TraceLogicalRingContextBcs,
                   AubMemDump::DataTypeHintValues::TraceLogicalRingContextVcs,
                   AubMemDump::DataTypeHintValues::TraceLogicalRingContextVecs,
                   AubMemDump::DataTypeHintValues::TraceCommandBuffer};

    for (uint32_t i = 0; i < 6; i++) {
        auto addressSpace = aubCsr->getAddressSpace(types[i]);
        EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceLocal, addressSpace);
    }

    auto addressSpace = aubCsr->getAddressSpace(AubMemDump::DataTypeHintValues::TraceNotype);
    EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceLocal, addressSpace);
}

HWTEST_F(AubCsrTest, WhenWriteWithAubManagerIsCalledThenAubManagerIsInvokedWithCorrectHint) {
    auto hwInfo = *NEO::defaultHwInfo.get();

    std::unique_ptr<ExecutionEnvironment> executionEnvironment(new ExecutionEnvironment);
    DeviceBitfield deviceBitfield(1);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    uint32_t rootDeviceIndex = 0u;

    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfo(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->aubCenter.reset(new AubCenter());

    executionEnvironment->initializeMemoryManager();
    auto allocation = executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::COMMAND_BUFFER});

    MockAubManager aubManager;
    std::unique_ptr<MockAubCsr<FamilyType>> aubCsr(new MockAubCsr<FamilyType>("", false, *executionEnvironment, rootDeviceIndex, deviceBitfield));
    aubCsr->aubManager = &aubManager;
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(aubCsr.get(),
                                                                                     EngineDescriptorHelper::getDefaultDescriptor({getChosenEngineType(hwInfo), EngineUsage::Regular},
                                                                                                                                  PreemptionHelper::getDefaultPreemptionMode(hwInfo)));
    aubCsr->setupContext(*osContext);

    aubCsr->writeMemoryWithAubManager(*allocation);
    EXPECT_TRUE(aubManager.writeMemory2Called);
    EXPECT_EQ(AubMemDump::DataTypeHintValues::TraceBatchBuffer, aubManager.hintToWriteMemory);

    aubManager.writeMemory2Called = false;

    auto allocation2 = executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::LINEAR_STREAM});

    aubCsr->writeMemoryWithAubManager(*allocation2);
    EXPECT_TRUE(aubManager.writeMemory2Called);
    EXPECT_EQ(AubMemDump::DataTypeHintValues::TraceNotype, aubManager.hintToWriteMemory);

    executionEnvironment->memoryManager->freeGraphicsMemory(allocation);
    executionEnvironment->memoryManager->freeGraphicsMemory(allocation2);
}

using HardwareContextContainerTests = ::testing::Test;

TEST_F(HardwareContextContainerTests, givenOsContextWithMultipleDevicesSupportedThenInitialzeHwContextsWithValidIndexes) {
    MockAubManager aubManager;
    MockOsContext osContext(1, EngineDescriptorHelper::getDefaultDescriptor(0b11));

    HardwareContextController hwContextControler(aubManager, osContext, 0);
    EXPECT_EQ(2u, hwContextControler.hardwareContexts.size());
    EXPECT_EQ(2u, osContext.getNumSupportedDevices());
    auto mockHwContext0 = static_cast<MockHardwareContext *>(hwContextControler.hardwareContexts[0].get());
    auto mockHwContext1 = static_cast<MockHardwareContext *>(hwContextControler.hardwareContexts[1].get());
    EXPECT_EQ(0u, mockHwContext0->deviceIndex);
    EXPECT_EQ(1u, mockHwContext1->deviceIndex);
}

TEST_F(HardwareContextContainerTests, givenSingleHwContextWhenSubmitMethodIsCalledOnHwContextControllerThenSubmitIsCalled) {
    MockAubManager aubManager;
    MockOsContext osContext(1, EngineDescriptorHelper::getDefaultDescriptor());
    HardwareContextController hwContextContainer(aubManager, osContext, 0);
    EXPECT_EQ(1u, hwContextContainer.hardwareContexts.size());

    auto mockHwContext0 = static_cast<MockHardwareContext *>(hwContextContainer.hardwareContexts[0].get());

    EXPECT_FALSE(mockHwContext0->writeAndSubmitCalled);
    EXPECT_FALSE(mockHwContext0->writeMemory2Called);

    hwContextContainer.submit(1, reinterpret_cast<const void *>(0x123), 2, 0, 1, false);

    EXPECT_TRUE(mockHwContext0->submitCalled);
    EXPECT_FALSE(mockHwContext0->writeAndSubmitCalled);
    EXPECT_FALSE(mockHwContext0->writeMemory2Called);
}

TEST_F(HardwareContextContainerTests, givenSingleHwContextWhenWriteMemoryIsCalledThenWholeMemoryBanksArePassed) {
    MockAubManager aubManager;
    MockOsContext osContext(1, EngineDescriptorHelper::getDefaultDescriptor());
    HardwareContextController hwContextContainer(aubManager, osContext, 0);
    EXPECT_EQ(1u, hwContextContainer.hardwareContexts.size());

    auto mockHwContext0 = static_cast<MockHardwareContext *>(hwContextContainer.hardwareContexts[0].get());

    aub_stream::AllocationParams params(1, reinterpret_cast<const void *>(0x123), 2, 3u, 4, 5);
    hwContextContainer.writeMemory(params);

    EXPECT_TRUE(mockHwContext0->writeMemory2Called);
    EXPECT_EQ(3u, mockHwContext0->memoryBanksPassed);
}

TEST_F(HardwareContextContainerTests, givenMultipleHwContextWhenSingleMethodIsCalledThenUseAllContexts) {
    MockAubManager aubManager;
    MockOsContext osContext(1, EngineDescriptorHelper::getDefaultDescriptor(0b11));
    HardwareContextController hwContextContainer(aubManager, osContext, 0);
    EXPECT_EQ(2u, hwContextContainer.hardwareContexts.size());

    auto mockHwContext0 = static_cast<MockHardwareContext *>(hwContextContainer.hardwareContexts[0].get());
    auto mockHwContext1 = static_cast<MockHardwareContext *>(hwContextContainer.hardwareContexts[1].get());

    EXPECT_FALSE(mockHwContext0->initializeCalled);
    EXPECT_FALSE(mockHwContext1->initializeCalled);
    EXPECT_FALSE(mockHwContext0->pollForCompletionCalled);
    EXPECT_FALSE(mockHwContext1->pollForCompletionCalled);
    EXPECT_FALSE(mockHwContext0->expectMemoryCalled);
    EXPECT_FALSE(mockHwContext1->expectMemoryCalled);
    EXPECT_FALSE(mockHwContext0->submitCalled);
    EXPECT_FALSE(mockHwContext1->submitCalled);
    EXPECT_FALSE(mockHwContext0->writeMemory2Called);
    EXPECT_FALSE(mockHwContext1->writeMemory2Called);

    aub_stream::AllocationParams params(1, reinterpret_cast<const void *>(0x123), 2, 3u, 4, 5);

    hwContextContainer.initialize();
    hwContextContainer.pollForCompletion();
    hwContextContainer.expectMemory(1, reinterpret_cast<const void *>(0x123), 2, 0);
    hwContextContainer.submit(1, reinterpret_cast<const void *>(0x123), 2, 0, 1, false);
    hwContextContainer.writeMemory(params);

    EXPECT_TRUE(mockHwContext0->initializeCalled);
    EXPECT_TRUE(mockHwContext1->initializeCalled);
    EXPECT_TRUE(mockHwContext0->pollForCompletionCalled);
    EXPECT_TRUE(mockHwContext1->pollForCompletionCalled);
    EXPECT_TRUE(mockHwContext0->expectMemoryCalled);
    EXPECT_TRUE(mockHwContext1->expectMemoryCalled);
    EXPECT_TRUE(mockHwContext0->submitCalled);
    EXPECT_TRUE(mockHwContext1->submitCalled);
    EXPECT_TRUE(mockHwContext0->writeMemory2Called);
    EXPECT_TRUE(mockHwContext1->writeMemory2Called);
    EXPECT_EQ(1u, mockHwContext0->memoryBanksPassed);
    EXPECT_EQ(2u, mockHwContext1->memoryBanksPassed);
}

TEST_F(HardwareContextContainerTests, givenHwContextWhenWriteMMIOIsCalledThenUseFirstContext) {
    MockAubManager aubManager;
    MockOsContext osContext(1, EngineDescriptorHelper::getDefaultDescriptor());
    HardwareContextController hwContextContainer(aubManager, osContext, 0);
    EXPECT_EQ(1u, hwContextContainer.hardwareContexts.size());

    auto mockHwContext = static_cast<MockHardwareContext *>(hwContextContainer.hardwareContexts[0].get());

    EXPECT_FALSE(mockHwContext->writeMMIOCalled);

    hwContextContainer.writeMMIO(0x01234567, 0x89ABCDEF);

    EXPECT_TRUE(mockHwContext->writeMMIOCalled);
}

TEST_F(HardwareContextContainerTests, givenMultipleHwContextWhenSingleMethodIsCalledThenUseFirstContext) {
    MockAubManager aubManager;
    MockOsContext osContext(1, EngineDescriptorHelper::getDefaultDescriptor(0b11));
    HardwareContextController hwContextContainer(aubManager, osContext, 0);
    EXPECT_EQ(2u, hwContextContainer.hardwareContexts.size());

    auto mockHwContext0 = static_cast<MockHardwareContext *>(hwContextContainer.hardwareContexts[0].get());
    auto mockHwContext1 = static_cast<MockHardwareContext *>(hwContextContainer.hardwareContexts[1].get());

    EXPECT_FALSE(mockHwContext0->dumpBufferBINCalled);
    EXPECT_FALSE(mockHwContext1->dumpBufferBINCalled);
    EXPECT_FALSE(mockHwContext0->dumpSurfaceCalled);
    EXPECT_FALSE(mockHwContext1->dumpSurfaceCalled);
    EXPECT_FALSE(mockHwContext0->readMemoryCalled);
    EXPECT_FALSE(mockHwContext1->readMemoryCalled);

    hwContextContainer.dumpBufferBIN(1, 2);
    hwContextContainer.dumpSurface({1, 2, 3, 4, 5, 6, 7, false, 0});
    hwContextContainer.readMemory(1, reinterpret_cast<void *>(0x123), 1, 2, 0);

    EXPECT_TRUE(mockHwContext0->dumpBufferBINCalled);
    EXPECT_FALSE(mockHwContext1->dumpBufferBINCalled);
    EXPECT_TRUE(mockHwContext0->dumpSurfaceCalled);
    EXPECT_FALSE(mockHwContext1->dumpSurfaceCalled);
    EXPECT_TRUE(mockHwContext0->readMemoryCalled);
    EXPECT_FALSE(mockHwContext1->readMemoryCalled);
}

using AubCommandStreamReceiverTests = Test<AubCommandStreamReceiverFixture>;

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationWritableWhenDumpAllocationIsCalledAndDumpFormatIsSpecifiedThenGraphicsAllocationShouldBeDumped) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    aubCsr.dumpAllocation(*gfxAllocation);

    EXPECT_TRUE(mockHardwareContext->dumpSurfaceCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenBcsEngineWhenDumpAllocationCalledThenIgnore) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular}));
    aubCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    aubCsr.dumpAllocation(*gfxAllocation);

    EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenCompressedGraphicsAllocationWritableWhenDumpAllocationIsCalledAndDumpFormatIsSpecifiedThenGraphicsAllocationShouldBeDumped) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpBufferFormat.set("TRE");

    auto gmmHelper = pDevice->executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();
    MockAubCenter *mockAubCenter = new MockAubCenter(defaultHwInfo.get(), *gmmHelper, false, "aubfile", CommandStreamReceiverType::CSR_AUB);
    mockAubCenter->aubManager = std::make_unique<MockAubManager>();

    pDevice->executionEnvironment->rootDeviceEnvironments[0]->aubCenter.reset(mockAubCenter);
    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    AllocationProperties properties(pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield());
    properties.flags.preferCompressed = true;

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    aubCsr.dumpAllocation(*gfxAllocation);

    EXPECT_TRUE(mockHardwareContext->dumpSurfaceCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationWritableWhenDumpAllocationIsCalledButDumpFormatIsNotSpecifiedThenGraphicsAllocationShouldNotBeDumped) {
    DebugManagerStateRestore dbgRestore;

    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});

    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    aubCsr.dumpAllocation(*gfxAllocation);

    EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationNonWritableWhenDumpAllocationIsCalledAndFormatIsSpecifiedThenGraphicsAllocationShouldNotBeDumped) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});

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

    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});

    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    gfxAllocation->setAllocDumpable(false, false);

    aubCsr.dumpAllocation(*gfxAllocation);

    EXPECT_FALSE(gfxAllocation->isAllocDumpable());
    EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationDumpableWhenDumpAllocationIsCalledAndAUBDumpAllocsOnEnqueueReadOnlyIsOnThenGraphicsAllocationShouldBeDumpedAndMarkedNonDumpable) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpAllocsOnEnqueueReadOnly.set(true);
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});

    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);

    auto &csrOsContext = aubCsr.getOsContext();

    {
        // Non-BCS engine, BCS dump
        EXPECT_FALSE(EngineHelpers::isBcs(csrOsContext.getEngineType()));
        gfxAllocation->setAllocDumpable(true, true);

        aubCsr.dumpAllocation(*gfxAllocation);

        EXPECT_TRUE(gfxAllocation->isAllocDumpable());
        EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);
    }

    {
        // Non-BCS engine, Non-BCS dump
        EXPECT_FALSE(EngineHelpers::isBcs(csrOsContext.getEngineType()));
        gfxAllocation->setAllocDumpable(true, false);

        aubCsr.dumpAllocation(*gfxAllocation);

        EXPECT_FALSE(gfxAllocation->isAllocDumpable());
        EXPECT_TRUE(mockHardwareContext->dumpSurfaceCalled);
        mockHardwareContext->dumpSurfaceCalled = false;
    }

    {
        // BCS engine, Non-BCS dump
        csrOsContext.getEngineType() = aub_stream::EngineType::ENGINE_BCS;
        EXPECT_TRUE(EngineHelpers::isBcs(csrOsContext.getEngineType()));
        gfxAllocation->setAllocDumpable(true, false);

        aubCsr.dumpAllocation(*gfxAllocation);

        EXPECT_TRUE(gfxAllocation->isAllocDumpable());
        EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);
    }

    {
        // BCS engine, BCS dump
        csrOsContext.getEngineType() = aub_stream::EngineType::ENGINE_BCS;
        EXPECT_TRUE(EngineHelpers::isBcs(csrOsContext.getEngineType()));
        gfxAllocation->setAllocDumpable(true, true);

        aubCsr.dumpAllocation(*gfxAllocation);

        EXPECT_FALSE(gfxAllocation->isAllocDumpable());
        EXPECT_TRUE(mockHardwareContext->dumpSurfaceCalled);
    }

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationWhenDumpAllocationIsCalledAndAUBDumpAllocsOnEnqueueSVMMemcpyOnlyIsSetThenDumpableFlagShouldBeRespected) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpAllocsOnEnqueueSVMMemcpyOnly.set(true);
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});

    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    gfxAllocation->setAllocDumpable(false, false);

    aubCsr.dumpAllocation(*gfxAllocation);
    EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);
    EXPECT_FALSE(gfxAllocation->isAllocDumpable());

    gfxAllocation->setAllocDumpable(true, false);
    aubCsr.dumpAllocation(*gfxAllocation);
    EXPECT_FALSE(gfxAllocation->isAllocDumpable());
    EXPECT_TRUE(mockHardwareContext->dumpSurfaceCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenGraphicsAllocationWritableWhenDumpAllocationIsCalledAndDumpFormatIsSpecifiedThenPollForCompletionShouldBeCalledBeforeGraphicsAllocationIsDumped) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);
    aubCsr.latestSentTaskCount = 1;

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    aubCsr.dumpAllocation(*gfxAllocation);

    EXPECT_TRUE(mockHardwareContext->pollForCompletionCalled);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubCommandStreamReceiverTests, givenUsmAllocationWhenDumpAllocationIsCalledAndDumpFormatIsSpecifiedThenUsmAllocationIsDumped) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockAubCsr<FamilyType> aubCsr("", true, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    aubCsr.setupContext(osContext);

    auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

    auto memoryManager = std::make_unique<MockMemoryManager>(false, true, *pDevice->executionEnvironment);
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager.get(), false);

    std::set<uint32_t> rootDeviceIndices{rootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{rootDeviceIndex, pDevice->getDeviceBitfield()}};

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = pDevice;
    auto ptr = svmManager->createUnifiedMemoryAllocation(4096, unifiedMemoryProperties);
    ASSERT_NE(nullptr, ptr);

    auto gfxAllocation = svmManager->getSVMAlloc(ptr)->gpuAllocations.getGraphicsAllocation(0);
    ASSERT_NE(nullptr, gfxAllocation);

    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    aubCsr.dumpAllocation(*gfxAllocation);

    EXPECT_TRUE(mockHardwareContext->dumpSurfaceCalled);

    svmManager->freeSVMAlloc(ptr);
}

using SimulatedCsrTest = ::testing::Test;
HWTEST_F(SimulatedCsrTest, givenAubCsrTypeWhenCreateCommandStreamReceiverThenProperAubCenterIsInitalized) {
    uint32_t expectedRootDeviceIndex = 10;
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.initializeMemoryManager();
    executionEnvironment.prepareRootDeviceEnvironments(expectedRootDeviceIndex + 2);

    auto rootDeviceEnvironment = new MockRootDeviceEnvironment(executionEnvironment);
    executionEnvironment.rootDeviceEnvironments[expectedRootDeviceIndex].reset(rootDeviceEnvironment);
    rootDeviceEnvironment->setHwInfo(defaultHwInfo.get());

    EXPECT_EQ(nullptr, executionEnvironment.rootDeviceEnvironments[expectedRootDeviceIndex]->aubCenter.get());
    EXPECT_FALSE(rootDeviceEnvironment->initAubCenterCalled);
    DeviceBitfield deviceBitfield(1);
    auto csr = std::make_unique<AUBCommandStreamReceiverHw<FamilyType>>("", true, executionEnvironment, expectedRootDeviceIndex, deviceBitfield);
    EXPECT_TRUE(rootDeviceEnvironment->initAubCenterCalled);
    EXPECT_NE(nullptr, executionEnvironment.rootDeviceEnvironments[expectedRootDeviceIndex]->aubCenter.get());
}

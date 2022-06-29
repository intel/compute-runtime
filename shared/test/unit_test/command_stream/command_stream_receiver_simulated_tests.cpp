/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/command_stream/command_stream_receiver_simulated_hw.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/hardware_context_controller.h"
#include "shared/source/memory_manager/memory_pool.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_csr_simulated_common_hw.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <array>
#include <memory>
using namespace NEO;

using CommandStreamSimulatedTests = HwHelperTest;

HWTEST_F(CommandStreamSimulatedTests, givenLocalMemoryAndAllocationWithStorageInfoNonZeroWhenMemoryBankIsQueriedThenBankForAllocationDeviceIsReturned) {
    ExecutionEnvironment executionEnvironment;
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(&hardwareInfo);
    executionEnvironment.initializeMemoryManager();

    MemoryAllocation allocation(0, AllocationType::UNKNOWN, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::LocalMemory, false, false, MemoryManager::maxOsContextCount);
    allocation.storageInfo.memoryBanks = 0x2u;

    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(executionEnvironment, 0, 1);

    if (csr->localMemoryEnabled) {
        auto bank = csr->getMemoryBank(&allocation);
        EXPECT_EQ(MemoryBanks::getBankForLocalMemory(1), bank);
    }
}

HWTEST_F(CommandStreamSimulatedTests, givenLocalMemoryAndNonLocalMemoryAllocationWithStorageInfoNonZeroWhenMemoryBankIsQueriedThenMainBankIsReturned) {
    ExecutionEnvironment executionEnvironment;
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(&hardwareInfo);

    executionEnvironment.initializeMemoryManager();
    MemoryAllocation allocation(0, AllocationType::UNKNOWN, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::System4KBPages, false, false, MemoryManager::maxOsContextCount);
    allocation.storageInfo.memoryBanks = 0x2u;

    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(executionEnvironment, 0, 1);
    auto bank = csr->getMemoryBank(&allocation);
    EXPECT_EQ(MemoryBanks::MainBank, bank);
}

HWTEST_F(CommandStreamSimulatedTests, givenLocalMemoryAndAllocationWithStorageInfoZeroWhenMemoryBankIsQueriedThenBankForCsrIsReturned) {
    ExecutionEnvironment executionEnvironment;
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(&hardwareInfo);
    executionEnvironment.initializeMemoryManager();

    MemoryAllocation allocation(0, AllocationType::UNKNOWN, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::LocalMemory, false, false, MemoryManager::maxOsContextCount);
    allocation.storageInfo.memoryBanks = 0x0u;

    DeviceBitfield deviceBitfield(0b100);
    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(executionEnvironment, 0, deviceBitfield);
    auto osContext = executionEnvironment.memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(deviceBitfield));
    csr->setupContext(*osContext);
    auto bank = csr->getMemoryBank(&allocation);
    EXPECT_EQ(MemoryBanks::getBankForLocalMemory(2), bank);
}

HWTEST_F(CommandStreamSimulatedTests, givenLocalMemoryAndNonLocalMemoryAllocationWithStorageInfoNonZeroWhenMemoryBanksBitfieldIsQueriedThenBanksBitfieldForSystemMemoryIsReturned) {
    ExecutionEnvironment executionEnvironment;
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(&hardwareInfo);
    executionEnvironment.initializeMemoryManager();

    MemoryAllocation allocation(0, AllocationType::UNKNOWN, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::System64KBPages, false, false, MemoryManager::maxOsContextCount);
    allocation.storageInfo.memoryBanks = 0x3u;

    DeviceBitfield deviceBitfield(1);
    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(executionEnvironment, 0, deviceBitfield);
    auto osContext = executionEnvironment.memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);
    auto banksBitfield = csr->getMemoryBanksBitfield(&allocation);
    EXPECT_TRUE(banksBitfield.none());
}

HWTEST_F(CommandStreamSimulatedTests, givenLocalMemoryNoncloneableAllocationWithManyBanksWhenMemoryBanksBitfieldIsQueriedThenSingleMemoryBankIsReturned) {
    ExecutionEnvironment executionEnvironment;
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(&hardwareInfo);
    executionEnvironment.initializeMemoryManager();

    MemoryAllocation allocation(0, AllocationType::UNKNOWN, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::LocalMemory, false, false, MemoryManager::maxOsContextCount);
    allocation.storageInfo.memoryBanks = 0x3u;
    allocation.storageInfo.cloningOfPageTables = false;

    DeviceBitfield deviceBitfield(0x1u);
    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(executionEnvironment, 0, deviceBitfield);
    auto osContext = executionEnvironment.memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);
    EXPECT_FALSE(csr->isMultiOsContextCapable());

    if (csr->localMemoryEnabled) {
        auto banksBitfield = csr->getMemoryBanksBitfield(&allocation);
        EXPECT_EQ(0x1lu, banksBitfield.to_ulong());
    }
}

HWTEST_F(CommandStreamSimulatedTests, givenLocalMemoryCloneableAllocationWithManyBanksWhenMemoryBanksBitfieldIsQueriedThenAllMemoryBanksAreReturned) {
    ExecutionEnvironment executionEnvironment;
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(&hardwareInfo);
    executionEnvironment.initializeMemoryManager();

    MemoryAllocation allocation(0, AllocationType::UNKNOWN, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::LocalMemory, false, false, MemoryManager::maxOsContextCount);
    allocation.storageInfo.memoryBanks = 0x3u;
    allocation.storageInfo.cloningOfPageTables = true;

    DeviceBitfield deviceBitfield(1);
    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(executionEnvironment, 0, deviceBitfield);
    EXPECT_FALSE(csr->isMultiOsContextCapable());
    auto osContext = executionEnvironment.memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);

    if (csr->localMemoryEnabled) {
        auto banksBitfield = csr->getMemoryBanksBitfield(&allocation);
        EXPECT_EQ(0x3lu, banksBitfield.to_ulong());
    }
}

HWTEST_F(CommandStreamSimulatedTests, givenLocalMemoryNoncloneableAllocationWithManyBanksWhenMemoryBanksBitfieldIsQueriedOnSpecialCsrThenAllMemoryBanksAreReturned) {
    ExecutionEnvironment executionEnvironment;
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(&hardwareInfo);
    executionEnvironment.initializeMemoryManager();

    MemoryAllocation allocation(0, AllocationType::UNKNOWN, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::LocalMemory, false, false, MemoryManager::maxOsContextCount);
    allocation.storageInfo.memoryBanks = 0x3u;
    allocation.storageInfo.cloningOfPageTables = false;

    DeviceBitfield deviceBitfield(0b11);
    MockSimulatedCsrHw<FamilyType> csr(executionEnvironment, 0, deviceBitfield);
    csr.multiOsContextCapable = true;
    EXPECT_TRUE(csr.isMultiOsContextCapable());
    auto osContext = executionEnvironment.memoryManager->createAndRegisterOsContext(&csr, EngineDescriptorHelper::getDefaultDescriptor(deviceBitfield));
    csr.setupContext(*osContext);

    if (csr.localMemoryEnabled) {
        auto banksBitfield = csr.getMemoryBanksBitfield(&allocation);
        EXPECT_EQ(0x3lu, banksBitfield.to_ulong());
    }
}

HWTEST_F(CommandStreamSimulatedTests, givenLocalMemoryAndAllocationWithStorageInfoZeroWhenMemoryBanksBitfieldIsQueriedThenBanksBitfieldForCsrDeviceIndexIsReturned) {
    ExecutionEnvironment executionEnvironment;
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(&hardwareInfo);
    executionEnvironment.initializeMemoryManager();

    MemoryAllocation allocation(0, AllocationType::UNKNOWN, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::LocalMemory, false, false, MemoryManager::maxOsContextCount);
    allocation.storageInfo.memoryBanks = 0x0u;

    DeviceBitfield deviceBitfield(0b100);
    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(executionEnvironment, 0, deviceBitfield);
    auto deviceIndex = 2u;

    auto osContext = executionEnvironment.memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(deviceBitfield));
    csr->setupContext(*osContext);
    auto banksBitfield = csr->getMemoryBanksBitfield(&allocation);
    EXPECT_EQ(1u, banksBitfield.count());
    EXPECT_TRUE(banksBitfield.test(deviceIndex));
}

HWTEST_F(CommandStreamSimulatedTests, givenLocalMemoryWhenSimulatedCsrGetAddressSpaceIsCalledWithDifferentHintsThenCorrectSpaceIsReturned) {
    ExecutionEnvironment executionEnvironment;
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(&hardwareInfo);
    executionEnvironment.initializeMemoryManager();

    std::array<uint32_t, 6> localMemoryHints = {AubMemDump::DataTypeHintValues::TraceLogicalRingContextRcs,
                                                AubMemDump::DataTypeHintValues::TraceLogicalRingContextCcs,
                                                AubMemDump::DataTypeHintValues::TraceLogicalRingContextBcs,
                                                AubMemDump::DataTypeHintValues::TraceLogicalRingContextVcs,
                                                AubMemDump::DataTypeHintValues::TraceLogicalRingContextVecs,
                                                AubMemDump::DataTypeHintValues::TraceCommandBuffer};

    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(executionEnvironment, 0, 1);

    if (csr->localMemoryEnabled) {
        for (const uint32_t hint : localMemoryHints) {
            EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceLocal, csr->getAddressSpace(hint));
        }
    }
    std::array<uint32_t, 1> nonLocalMemoryHints = {AubMemDump::DataTypeHintValues::TraceNotype};

    for (const uint32_t hint : nonLocalMemoryHints) {
        EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceNonlocal, csr->getAddressSpace(hint));
    }
}

HWTEST_F(CommandStreamSimulatedTests, givenLocalMemoryDisabledWhenSimulatedCsrGetAddressSpaceIsCalledWithDifferentHintsThenCorrectSpaceIsReturned) {
    ExecutionEnvironment executionEnvironment;
    hardwareInfo.featureTable.flags.ftrLocalMemory = false;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(&hardwareInfo);
    executionEnvironment.initializeMemoryManager();

    std::array<uint32_t, 7> nonLocalMemoryHints = {AubMemDump::DataTypeHintValues::TraceNotype,
                                                   AubMemDump::DataTypeHintValues::TraceLogicalRingContextRcs,
                                                   AubMemDump::DataTypeHintValues::TraceLogicalRingContextCcs,
                                                   AubMemDump::DataTypeHintValues::TraceLogicalRingContextBcs,
                                                   AubMemDump::DataTypeHintValues::TraceLogicalRingContextVcs,
                                                   AubMemDump::DataTypeHintValues::TraceLogicalRingContextVecs,
                                                   AubMemDump::DataTypeHintValues::TraceCommandBuffer};

    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(executionEnvironment, 0, 1);

    for (const uint32_t hint : nonLocalMemoryHints) {
        EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceNonlocal, csr->getAddressSpace(hint));
    }
}

HWTEST_F(CommandStreamSimulatedTests, givenAUBDumpForceAllToLocalMemoryWhenSimulatedCsrGetAddressSpaceIsCalledWithDifferentHintsThenTraceLocalIsReturned) {
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.AUBDumpForceAllToLocalMemory.set(true);

    hardwareInfo.featureTable.flags.ftrLocalMemory = false;
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(&hardwareInfo);
    executionEnvironment.initializeMemoryManager();

    std::array<uint32_t, 7> localMemoryHints = {AubMemDump::DataTypeHintValues::TraceNotype,
                                                AubMemDump::DataTypeHintValues::TraceLogicalRingContextRcs,
                                                AubMemDump::DataTypeHintValues::TraceLogicalRingContextCcs,
                                                AubMemDump::DataTypeHintValues::TraceLogicalRingContextBcs,
                                                AubMemDump::DataTypeHintValues::TraceLogicalRingContextVcs,
                                                AubMemDump::DataTypeHintValues::TraceLogicalRingContextVecs,
                                                AubMemDump::DataTypeHintValues::TraceCommandBuffer};

    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(executionEnvironment, 0, 1);

    if (csr->localMemoryEnabled) {
        for (const uint32_t hint : localMemoryHints) {
            EXPECT_EQ(AubMemDump::AddressSpaceValues::TraceLocal, csr->getAddressSpace(hint));
        }
    }
}

HWTEST_F(CommandStreamSimulatedTests, givenMultipleBitsInStorageInfoWhenQueryingDeviceIndexThenLowestDeviceIndexIsReturned) {
    StorageInfo storageInfo;

    storageInfo.memoryBanks = {1u | (1u << 2u)};
    auto deviceIndex = CommandStreamReceiverSimulatedHw<FamilyType>::getDeviceIndexFromStorageInfo(storageInfo);
    EXPECT_EQ(0u, deviceIndex);

    storageInfo.memoryBanks = (1u << 2u) | (1u << 3u);

    deviceIndex = CommandStreamReceiverSimulatedHw<FamilyType>::getDeviceIndexFromStorageInfo(storageInfo);
    EXPECT_EQ(2u, deviceIndex);
}

HWTEST_F(CommandStreamSimulatedTests, givenSingleBitInStorageInfoWhenQueryingDeviceIndexThenCorrectDeviceIndexIsReturned) {

    StorageInfo storageInfo;

    for (uint32_t i = 0; i < 4u; i++) {
        storageInfo.memoryBanks.reset();
        storageInfo.memoryBanks.set(i);
        auto deviceIndex = CommandStreamReceiverSimulatedHw<FamilyType>::getDeviceIndexFromStorageInfo(storageInfo);
        EXPECT_EQ(i, deviceIndex);
    }
}

HWTEST_F(CommandStreamSimulatedTests, givenSimulatedCommandStreamReceiverWhenCloningPageTableIsRequiredThenAubManagerIsUsedForWriteMemory) {
    auto mockManager = std::make_unique<MockAubManager>();

    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr->aubManager = mockManager.get();
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(csr->hardwareContextController->hardwareContexts[0].get());

    int dummy = 1;
    GraphicsAllocation graphicsAllocation{0, AllocationType::UNKNOWN,
                                          &dummy, 0, 0, sizeof(dummy), MemoryPool::MemoryNull, MemoryManager::maxOsContextCount};
    graphicsAllocation.storageInfo.cloningOfPageTables = true;
    csr->writeMemoryWithAubManager(graphicsAllocation);

    EXPECT_FALSE(mockHardwareContext->writeMemory2Called);
    EXPECT_TRUE(mockManager->writeMemory2Called);
}

HWTEST_F(CommandStreamSimulatedTests, givenCompressedAllocationWhenCloningPageTableIsRequiredThenAubManagerIsUsedForWriteMemory) {
    auto mockManager = std::make_unique<MockAubManager>();
    mockManager->storeAllocationParams = true;

    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr->aubManager = mockManager.get();
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(csr->hardwareContextController->hardwareContexts[0].get());

    MockGmm gmm(csr->peekGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    gmm.isCompressionEnabled = true;

    int dummy = 1;
    GraphicsAllocation graphicsAllocation{0, AllocationType::UNKNOWN,
                                          &dummy, 0, 0, sizeof(dummy), MemoryPool::MemoryNull, MemoryManager::maxOsContextCount};
    graphicsAllocation.storageInfo.cloningOfPageTables = true;

    graphicsAllocation.setDefaultGmm(&gmm);

    csr->writeMemoryWithAubManager(graphicsAllocation);

    EXPECT_FALSE(mockHardwareContext->writeMemory2Called);
    EXPECT_TRUE(mockManager->writeMemory2Called);

    EXPECT_EQ(1u, mockManager->storedAllocationParams.size());
    EXPECT_TRUE(mockManager->storedAllocationParams[0].additionalParams.compressionEnabled);
    EXPECT_FALSE(mockManager->storedAllocationParams[0].additionalParams.uncached);
}

HWTEST_F(CommandStreamSimulatedTests, givenUncachedAllocationWhenCloningPageTableIsRequiredThenAubManagerIsUsedForWriteMemory) {
    auto mockManager = std::make_unique<MockAubManager>();
    mockManager->storeAllocationParams = true;

    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr->aubManager = mockManager.get();
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(csr->hardwareContextController->hardwareContexts[0].get());

    MockGmm gmm(csr->peekGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED, false, {}, true);
    gmm.isCompressionEnabled = false;

    int dummy = 1;
    GraphicsAllocation graphicsAllocation{0, AllocationType::UNKNOWN,
                                          &dummy, 0, 0, sizeof(dummy), MemoryPool::MemoryNull, MemoryManager::maxOsContextCount};
    graphicsAllocation.storageInfo.cloningOfPageTables = true;

    graphicsAllocation.setDefaultGmm(&gmm);

    csr->writeMemoryWithAubManager(graphicsAllocation);

    EXPECT_FALSE(mockHardwareContext->writeMemory2Called);
    EXPECT_TRUE(mockManager->writeMemory2Called);

    EXPECT_EQ(1u, mockManager->storedAllocationParams.size());
    EXPECT_FALSE(mockManager->storedAllocationParams[0].additionalParams.compressionEnabled);
    EXPECT_TRUE(mockManager->storedAllocationParams[0].additionalParams.uncached);
}

HWTEST_F(CommandStreamSimulatedTests, givenTileInstancedAllocationWhenWriteMemoryWithAubManagerThenEachHardwareContextGetsDifferentMemoryBank) {
    auto mockManager = std::make_unique<MockAubManager>();

    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr->aubManager = mockManager.get();
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(0b11));
    csr->hardwareContextController = std::make_unique<HardwareContextController>(*mockManager, osContext, 0);
    auto firstMockHardwareContext = static_cast<MockHardwareContext *>(csr->hardwareContextController->hardwareContexts[0].get());
    auto secondMockHardwareContext = static_cast<MockHardwareContext *>(csr->hardwareContextController->hardwareContexts[1].get());
    csr->multiOsContextCapable = true;

    int dummy = 1;
    GraphicsAllocation graphicsAllocation{0, AllocationType::UNKNOWN,
                                          &dummy, 0, 0, sizeof(dummy), MemoryPool::LocalMemory, MemoryManager::maxOsContextCount};
    graphicsAllocation.storageInfo.cloningOfPageTables = false;
    graphicsAllocation.storageInfo.tileInstanced = true;
    graphicsAllocation.storageInfo.memoryBanks = 0b11u;
    csr->writeMemoryWithAubManager(graphicsAllocation);

    EXPECT_TRUE(firstMockHardwareContext->writeMemory2Called);
    EXPECT_EQ(0b01u, firstMockHardwareContext->memoryBanksPassed);
    EXPECT_TRUE(secondMockHardwareContext->writeMemory2Called);
    EXPECT_EQ(0b10u, secondMockHardwareContext->memoryBanksPassed);
    EXPECT_FALSE(mockManager->writeMemory2Called);
}

HWTEST_F(CommandStreamSimulatedTests, givenCompressedTileInstancedAllocationWhenWriteMemoryWithAubManagerThenEachHardwareContextGetsCompressionInfo) {
    auto mockManager = std::make_unique<MockAubManager>();

    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr->aubManager = mockManager.get();
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(0b11));
    csr->hardwareContextController = std::make_unique<HardwareContextController>(*mockManager, osContext, 0);
    auto firstMockHardwareContext = static_cast<MockHardwareContext *>(csr->hardwareContextController->hardwareContexts[0].get());
    firstMockHardwareContext->storeAllocationParams = true;
    auto secondMockHardwareContext = static_cast<MockHardwareContext *>(csr->hardwareContextController->hardwareContexts[1].get());
    secondMockHardwareContext->storeAllocationParams = true;

    csr->multiOsContextCapable = true;

    MockGmm gmm(csr->peekGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    gmm.isCompressionEnabled = true;

    int dummy = 1;
    GraphicsAllocation graphicsAllocation{0, AllocationType::UNKNOWN,
                                          &dummy, 0, 0, sizeof(dummy), MemoryPool::LocalMemory, MemoryManager::maxOsContextCount};
    graphicsAllocation.storageInfo.cloningOfPageTables = false;
    graphicsAllocation.storageInfo.tileInstanced = true;
    graphicsAllocation.storageInfo.memoryBanks = 0b11u;

    graphicsAllocation.setDefaultGmm(&gmm);

    csr->writeMemoryWithAubManager(graphicsAllocation);

    EXPECT_TRUE(firstMockHardwareContext->writeMemory2Called);
    EXPECT_EQ(1u, firstMockHardwareContext->storedAllocationParams.size());
    EXPECT_TRUE(firstMockHardwareContext->storedAllocationParams[0].additionalParams.compressionEnabled);
    EXPECT_FALSE(firstMockHardwareContext->storedAllocationParams[0].additionalParams.uncached);

    EXPECT_TRUE(secondMockHardwareContext->writeMemory2Called);
    EXPECT_EQ(1u, secondMockHardwareContext->storedAllocationParams.size());
    EXPECT_TRUE(secondMockHardwareContext->storedAllocationParams[0].additionalParams.compressionEnabled);
    EXPECT_FALSE(secondMockHardwareContext->storedAllocationParams[0].additionalParams.uncached);
}

HWTEST_F(CommandStreamSimulatedTests, givenUncachedTileInstancedAllocationWhenWriteMemoryWithAubManagerThenEachHardwareContextGetsCompressionInfo) {
    auto mockManager = std::make_unique<MockAubManager>();

    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr->aubManager = mockManager.get();
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(0b11));
    csr->hardwareContextController = std::make_unique<HardwareContextController>(*mockManager, osContext, 0);
    auto firstMockHardwareContext = static_cast<MockHardwareContext *>(csr->hardwareContextController->hardwareContexts[0].get());
    firstMockHardwareContext->storeAllocationParams = true;
    auto secondMockHardwareContext = static_cast<MockHardwareContext *>(csr->hardwareContextController->hardwareContexts[1].get());
    secondMockHardwareContext->storeAllocationParams = true;

    csr->multiOsContextCapable = true;

    MockGmm gmm(csr->peekGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED, false, {}, true);
    gmm.isCompressionEnabled = false;

    int dummy = 1;
    GraphicsAllocation graphicsAllocation{0, AllocationType::UNKNOWN,
                                          &dummy, 0, 0, sizeof(dummy), MemoryPool::LocalMemory, MemoryManager::maxOsContextCount};
    graphicsAllocation.storageInfo.cloningOfPageTables = false;
    graphicsAllocation.storageInfo.tileInstanced = true;
    graphicsAllocation.storageInfo.memoryBanks = 0b11u;

    graphicsAllocation.setDefaultGmm(&gmm);

    csr->writeMemoryWithAubManager(graphicsAllocation);

    EXPECT_TRUE(firstMockHardwareContext->writeMemory2Called);
    EXPECT_EQ(1u, firstMockHardwareContext->storedAllocationParams.size());
    EXPECT_FALSE(firstMockHardwareContext->storedAllocationParams[0].additionalParams.compressionEnabled);
    EXPECT_TRUE(firstMockHardwareContext->storedAllocationParams[0].additionalParams.uncached);

    EXPECT_TRUE(secondMockHardwareContext->writeMemory2Called);
    EXPECT_EQ(1u, secondMockHardwareContext->storedAllocationParams.size());
    EXPECT_FALSE(secondMockHardwareContext->storedAllocationParams[0].additionalParams.compressionEnabled);
    EXPECT_TRUE(secondMockHardwareContext->storedAllocationParams[0].additionalParams.uncached);
}

HWTEST_F(CommandStreamSimulatedTests, givenTileInstancedAllocationWithMissingMemoryBankWhenWriteMemoryWithAubManagerThenAbortIsCalled) {
    auto mockManager = std::make_unique<MockAubManager>();

    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr->aubManager = mockManager.get();
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(0b11));
    csr->hardwareContextController = std::make_unique<HardwareContextController>(*mockManager, osContext, 0);
    auto firstMockHardwareContext = static_cast<MockHardwareContext *>(csr->hardwareContextController->hardwareContexts[0].get());
    auto secondMockHardwareContext = static_cast<MockHardwareContext *>(csr->hardwareContextController->hardwareContexts[1].get());
    csr->multiOsContextCapable = true;

    int dummy = 1;
    GraphicsAllocation graphicsAllocation{0, AllocationType::UNKNOWN,
                                          &dummy, 0, 0, sizeof(dummy), MemoryPool::LocalMemory, MemoryManager::maxOsContextCount};
    graphicsAllocation.storageInfo.cloningOfPageTables = false;
    graphicsAllocation.storageInfo.tileInstanced = true;
    graphicsAllocation.storageInfo.memoryBanks = 2u;
    EXPECT_THROW(csr->writeMemoryWithAubManager(graphicsAllocation), std::exception);
    EXPECT_FALSE(firstMockHardwareContext->writeMemory2Called);
    EXPECT_FALSE(secondMockHardwareContext->writeMemory2Called);
}

HWTEST_F(CommandStreamSimulatedTests, givenCommandBufferAllocationWhenWriteMemoryCalledThenHintIsPassed) {
    auto mockManager = std::make_unique<MockAubManager>();
    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr->aubManager = mockManager.get();

    int dummy = 1;
    GraphicsAllocation graphicsAllocation{0, AllocationType::COMMAND_BUFFER,
                                          &dummy, 0, 0, sizeof(dummy), MemoryPool::MemoryNull, MemoryManager::maxOsContextCount};
    graphicsAllocation.storageInfo.cloningOfPageTables = true;
    csr->writeMemoryWithAubManager(graphicsAllocation);

    EXPECT_EQ(AubMemDump::DataTypeHintValues::TraceBatchBuffer, mockManager->hintToWriteMemory);
    EXPECT_TRUE(mockManager->writeMemory2Called);
}

HWTEST_F(CommandStreamSimulatedTests, givenSpecificMemoryPoolAllocationWhenWriteMemoryByAubManagerOrHardwareContextIsCalledThenCorrectPageSizeIsPassed) {
    auto mockManager = std::make_unique<MockAubManager>();
    auto csr = std::make_unique<MockSimulatedCsrHw<FamilyType>>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr->aubManager = mockManager.get();

    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor());
    csr->hardwareContextController = std::make_unique<HardwareContextController>(*mockManager, osContext, 0);
    csr->setupContext(osContext);
    auto mockHardwareContext = static_cast<MockHardwareContext *>(csr->hardwareContextController->hardwareContexts[0].get());

    int dummy = 1;

    MemoryPool poolsWith4kPages[] = {
        MemoryPool::System4KBPages,
        MemoryPool::System4KBPagesWith32BitGpuAddressing,
        MemoryPool::SystemCpuInaccessible};

    for (size_t i = 0; i < arrayCount(poolsWith4kPages); i++) {

        mockManager->writeMemoryPageSizePassed = 0;
        mockManager->writeMemory2Called = false;

        mockHardwareContext->writeMemoryPageSizePassed = 0;
        mockHardwareContext->writeMemory2Called = false;

        GraphicsAllocation graphicsAllocation{0, AllocationType::COMMAND_BUFFER,
                                              &dummy, 0, 0, sizeof(dummy), poolsWith4kPages[i], MemoryManager::maxOsContextCount};
        graphicsAllocation.storageInfo.cloningOfPageTables = true;
        csr->writeMemoryWithAubManager(graphicsAllocation);

        EXPECT_TRUE(mockManager->writeMemory2Called);
        EXPECT_EQ(MemoryConstants::pageSize, mockManager->writeMemoryPageSizePassed);

        graphicsAllocation.storageInfo.cloningOfPageTables = false;
        csr->writeMemoryWithAubManager(graphicsAllocation);

        if (graphicsAllocation.isAllocatedInLocalMemoryPool()) {
            EXPECT_TRUE(mockHardwareContext->writeMemory2Called);
            EXPECT_EQ(MemoryConstants::pageSize, mockHardwareContext->writeMemoryPageSizePassed);
        } else {
            EXPECT_TRUE(mockManager->writeMemory2Called);
            EXPECT_EQ(MemoryConstants::pageSize, mockManager->writeMemoryPageSizePassed);
        }
    }

    MemoryPool poolsWith64kPages[] = {
        MemoryPool::System64KBPages,
        MemoryPool::System64KBPagesWith32BitGpuAddressing,
        MemoryPool::LocalMemory};

    for (size_t i = 0; i < arrayCount(poolsWith64kPages); i++) {

        mockManager->writeMemoryPageSizePassed = 0;
        mockManager->writeMemory2Called = false;

        mockHardwareContext->writeMemoryPageSizePassed = 0;
        mockHardwareContext->writeMemory2Called = false;

        GraphicsAllocation graphicsAllocation{0, AllocationType::COMMAND_BUFFER,
                                              &dummy, 0, 0, sizeof(dummy), poolsWith64kPages[i], MemoryManager::maxOsContextCount};
        graphicsAllocation.storageInfo.cloningOfPageTables = true;
        csr->writeMemoryWithAubManager(graphicsAllocation);

        EXPECT_TRUE(mockManager->writeMemory2Called);
        EXPECT_EQ(MemoryConstants::pageSize64k, mockManager->writeMemoryPageSizePassed);

        graphicsAllocation.storageInfo.cloningOfPageTables = false;
        csr->writeMemoryWithAubManager(graphicsAllocation);

        if (graphicsAllocation.isAllocatedInLocalMemoryPool()) {
            EXPECT_TRUE(mockHardwareContext->writeMemory2Called);
            EXPECT_EQ(MemoryConstants::pageSize64k, mockHardwareContext->writeMemoryPageSizePassed);
        } else {
            EXPECT_TRUE(mockManager->writeMemory2Called);
            EXPECT_EQ(MemoryConstants::pageSize64k, mockManager->writeMemoryPageSizePassed);
        }
    }
}

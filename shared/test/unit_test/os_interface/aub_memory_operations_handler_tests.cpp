/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/aub_memory_operations_handler_tests.h"

#include "shared/source/aub/aub_helper.h"
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/hw_test.h"

TEST_F(AubMemoryOperationsHandlerTests, givenNullPtrAsAubManagerWhenMakeResidentCalledThenFalseReturned) {
    getMemoryOperationsHandler()->setAubManager(nullptr);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    auto result = memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    EXPECT_EQ(result, MemoryOperationsStatus::deviceUninitialized);
}

TEST_F(AubMemoryOperationsHandlerTests, givenAubManagerWhenMakeResidentCalledThenTrueReturnedAndWriteCalled) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    auto result = memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    EXPECT_EQ(result, MemoryOperationsStatus::success);
    EXPECT_TRUE(aubManager.writeMemory2Called);

    auto itor = std::find(memoryOperationsInterface->residentAllocations.begin(), memoryOperationsInterface->residentAllocations.end(), allocPtr);
    EXPECT_NE(memoryOperationsInterface->residentAllocations.end(), itor);
    EXPECT_EQ(1u, memoryOperationsInterface->residentAllocations.size());

    aubManager.writeMemory2Called = false;

    result = memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    EXPECT_EQ(result, MemoryOperationsStatus::success);
    EXPECT_TRUE(aubManager.writeMemory2Called);

    itor = std::find(memoryOperationsInterface->residentAllocations.begin(), memoryOperationsInterface->residentAllocations.end(), allocPtr);
    EXPECT_NE(memoryOperationsInterface->residentAllocations.end(), itor);
    EXPECT_EQ(2u, memoryOperationsInterface->residentAllocations.size());
}

TEST_F(AubMemoryOperationsHandlerTests, givenAubManagerWhenCallingLockThenTrueReturnedAndWriteCalled) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    auto result = memoryOperationsInterface->lock(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1));
    EXPECT_EQ(result, MemoryOperationsStatus::success);
    EXPECT_TRUE(aubManager.writeMemory2Called);

    auto itor = std::find(memoryOperationsInterface->residentAllocations.begin(), memoryOperationsInterface->residentAllocations.end(), allocPtr);
    EXPECT_NE(memoryOperationsInterface->residentAllocations.end(), itor);
    EXPECT_EQ(1u, memoryOperationsInterface->residentAllocations.size());

    aubManager.writeMemory2Called = false;

    result = memoryOperationsInterface->lock(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1));
    EXPECT_EQ(result, MemoryOperationsStatus::success);
    EXPECT_TRUE(aubManager.writeMemory2Called);

    itor = std::find(memoryOperationsInterface->residentAllocations.begin(), memoryOperationsInterface->residentAllocations.end(), allocPtr);
    EXPECT_NE(memoryOperationsInterface->residentAllocations.end(), itor);
    EXPECT_EQ(2u, memoryOperationsInterface->residentAllocations.size());
}

TEST_F(AubMemoryOperationsHandlerTests, givenAubManagerAndAllocationOfOneTimeAubWritableAllocationTypeWhenMakeResidentCalledTwoTimesThenWriteMemoryOnce) {
    ASSERT_TRUE(AubHelper::isOneTimeAubWritableAllocationType(AllocationType::buffer));
    allocPtr->setAllocationType(AllocationType::buffer);

    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    auto result = memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    EXPECT_EQ(result, MemoryOperationsStatus::success);
    EXPECT_TRUE(aubManager.writeMemory2Called);

    auto itor = std::find(memoryOperationsInterface->residentAllocations.begin(), memoryOperationsInterface->residentAllocations.end(), allocPtr);
    EXPECT_NE(memoryOperationsInterface->residentAllocations.end(), itor);
    EXPECT_EQ(1u, memoryOperationsInterface->residentAllocations.size());

    aubManager.writeMemory2Called = false;

    result = memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    EXPECT_EQ(result, MemoryOperationsStatus::success);
    EXPECT_FALSE(aubManager.writeMemory2Called);

    itor = std::find(memoryOperationsInterface->residentAllocations.begin(), memoryOperationsInterface->residentAllocations.end(), allocPtr);
    EXPECT_NE(memoryOperationsInterface->residentAllocations.end(), itor);
    EXPECT_EQ(1u, memoryOperationsInterface->residentAllocations.size());
}

TEST_F(AubMemoryOperationsHandlerTests, givenAubManagerWhenMakeResidentCalledOnWriteOnlyAllocationThenTrueReturnedAndWriteCalled) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();

    allocPtr->setWriteMemoryOnly(true);

    auto result = memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    EXPECT_EQ(result, MemoryOperationsStatus::success);
    EXPECT_TRUE(aubManager.writeMemory2Called);

    auto itor = std::find(memoryOperationsInterface->residentAllocations.begin(), memoryOperationsInterface->residentAllocations.end(), allocPtr);
    EXPECT_EQ(memoryOperationsInterface->residentAllocations.end(), itor);
    EXPECT_EQ(0u, memoryOperationsInterface->residentAllocations.size());

    aubManager.writeMemory2Called = false;
    result = memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    EXPECT_EQ(result, MemoryOperationsStatus::success);
    EXPECT_TRUE(aubManager.writeMemory2Called);

    itor = std::find(memoryOperationsInterface->residentAllocations.begin(), memoryOperationsInterface->residentAllocations.end(), allocPtr);
    EXPECT_EQ(memoryOperationsInterface->residentAllocations.end(), itor);
    EXPECT_EQ(0u, memoryOperationsInterface->residentAllocations.size());
}

TEST_F(AubMemoryOperationsHandlerTests, givenAubManagerAndAllocationInLocalMemoryPoolWithoutPageTablesCloningWhenMakeResidentCalledThenWriteMemoryAubIsCalled) {
    MemoryAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::localMemory, false, false, MemoryManager::maxOsContextCount);
    allocPtr = &allocation;
    allocPtr->storageInfo.cloningOfPageTables = false;

    DeviceBitfield deviceBitfield(1);
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), 0, deviceBitfield);
    csr->localMemoryEnabled = true;

    auto oldCsr = device->getDefaultEngine().commandStreamReceiver;
    device->getDefaultEngine().commandStreamReceiver = csr.get();

    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    auto result = memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    EXPECT_EQ(result, MemoryOperationsStatus::success);
    EXPECT_FALSE(aubManager.writeMemory2Called);
    EXPECT_EQ(1u, csr->writeMemoryAubCalled);

    auto itor = std::find(memoryOperationsInterface->residentAllocations.begin(), memoryOperationsInterface->residentAllocations.end(), allocPtr);
    EXPECT_NE(memoryOperationsInterface->residentAllocations.end(), itor);
    EXPECT_EQ(1u, memoryOperationsInterface->residentAllocations.size());

    device->getDefaultEngine().commandStreamReceiver = oldCsr;
}

TEST_F(AubMemoryOperationsHandlerTests, givenAubManagerWhenMakeResidentCalledThenInitializeEngineBeforeWriteMemory) {
    DeviceBitfield deviceBitfield(1);
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), 0, deviceBitfield);
    csr->localMemoryEnabled = true;

    auto oldCsr = device->getDefaultEngine().commandStreamReceiver;
    device->getDefaultEngine().commandStreamReceiver = csr.get();

    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    auto result = memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    EXPECT_EQ(result, MemoryOperationsStatus::success);
    EXPECT_EQ(1u, csr->initializeEngineCalled);

    auto itor = std::find(memoryOperationsInterface->residentAllocations.begin(), memoryOperationsInterface->residentAllocations.end(), allocPtr);
    EXPECT_NE(memoryOperationsInterface->residentAllocations.end(), itor);
    EXPECT_EQ(1u, memoryOperationsInterface->residentAllocations.size());

    device->getDefaultEngine().commandStreamReceiver = oldCsr;
}

TEST_F(AubMemoryOperationsHandlerTests, givenAubManagerWhenMakeResidentCalledWithNullDeviceThenDoNotInitializeEngineBeforeWriteMemory) {
    DeviceBitfield deviceBitfield(1);
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), 0, deviceBitfield);
    csr->localMemoryEnabled = true;

    auto oldCsr = device->getDefaultEngine().commandStreamReceiver;
    device->getDefaultEngine().commandStreamReceiver = csr.get();

    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    auto result = memoryOperationsInterface->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    EXPECT_EQ(result, MemoryOperationsStatus::success);
    EXPECT_EQ(0u, csr->initializeEngineCalled);

    device->getDefaultEngine().commandStreamReceiver = oldCsr;
}

TEST_F(AubMemoryOperationsHandlerTests, givenAubManagerWhenMakeResidentCalledOnCompressedAllocationThenPassCorrectParams) {
    MockAubManager aubManager;
    aubManager.storeAllocationParams = true;

    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u));
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    MockGmm gmm(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper());
    gmm.setCompressionEnabled(true);
    allocPtr->setDefaultGmm(&gmm);

    auto result = memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    EXPECT_EQ(result, MemoryOperationsStatus::success);

    EXPECT_TRUE(aubManager.writeMemory2Called);
    EXPECT_EQ(1u, aubManager.storedAllocationParams.size());
    EXPECT_TRUE(aubManager.storedAllocationParams[0].additionalParams.compressionEnabled);
    EXPECT_FALSE(aubManager.storedAllocationParams[0].additionalParams.uncached);
}

TEST_F(AubMemoryOperationsHandlerTests, givenAubManagerWhenMakeResidentCalledOnUncachedAllocationThenPassCorrectParams) {
    MockAubManager aubManager;
    aubManager.storeAllocationParams = true;

    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u));
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    MockGmm gmm(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED, {}, gmmRequirements);
    gmm.setCompressionEnabled(false);
    allocPtr->setDefaultGmm(&gmm);

    auto result = memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    EXPECT_EQ(result, MemoryOperationsStatus::success);

    EXPECT_TRUE(aubManager.writeMemory2Called);
    EXPECT_EQ(1u, aubManager.storedAllocationParams.size());
    EXPECT_FALSE(aubManager.storedAllocationParams[0].additionalParams.compressionEnabled);
    EXPECT_TRUE(aubManager.storedAllocationParams[0].additionalParams.uncached);
}

TEST_F(AubMemoryOperationsHandlerTests, givenAllocationWhenMakeResidentCalledThenTraceNotypeHintReturned) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    EXPECT_EQ(aubManager.hintToWriteMemory, AubMemDump::DataTypeHintValues::TraceNotype);
}
TEST_F(AubMemoryOperationsHandlerTests, givenNonResidentAllocationWhenIsResidentCalledThenFalseReturned) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    auto result = memoryOperationsInterface->isResident(nullptr, allocation);
    EXPECT_EQ(result, MemoryOperationsStatus::memoryNotFound);
}
TEST_F(AubMemoryOperationsHandlerTests, givenResidentAllocationWhenIsResidentCalledThenTrueReturned) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    auto result = memoryOperationsInterface->isResident(nullptr, allocation);
    EXPECT_EQ(result, MemoryOperationsStatus::success);
}
TEST_F(AubMemoryOperationsHandlerTests, givenNonResidentAllocationWhenEvictCalledThenFalseReturned) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    auto result = memoryOperationsInterface->evict(nullptr, allocation);
    EXPECT_EQ(result, MemoryOperationsStatus::memoryNotFound);
}
TEST_F(AubMemoryOperationsHandlerTests, givenResidentAllocationWhenEvictCalledThenTrueReturned) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    auto result = memoryOperationsInterface->evict(nullptr, allocation);
    EXPECT_EQ(result, MemoryOperationsStatus::success);
}

TEST_F(AubMemoryOperationsHandlerTests, givenLocalMemoryAndNonLocalMemoryAllocationWithStorageInfoNonZeroWhenGetMemoryBanksBitfieldThenBanksBitfieldForSystemMemoryIsReturned) {
    auto executionEnvironment = device->getExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hardwareInfo);
    executionEnvironment->initializeMemoryManager();

    MemoryAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::system64KBPages, false, false, MemoryManager::maxOsContextCount);
    allocation.storageInfo.memoryBanks = 0x3u;

    DeviceBitfield deviceBitfield(1);
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), 0, deviceBitfield);
    csr->localMemoryEnabled = true;
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);

    auto oldCsr = device->getDefaultEngine().commandStreamReceiver;
    device->getDefaultEngine().commandStreamReceiver = csr.get();

    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();

    auto banksBitfield = memoryOperationsInterface->getMemoryBanksBitfield(&allocation, device.get());

    EXPECT_TRUE(banksBitfield.none());

    device->getDefaultEngine().commandStreamReceiver = oldCsr;
}

TEST_F(AubMemoryOperationsHandlerTests, givenLocalMemoryNoncloneableAllocationWithManyBanksWhenGetMemoryBanksBitfieldThenSingleMemoryBankIsReturned) {
    auto executionEnvironment = device->getExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hardwareInfo);
    executionEnvironment->initializeMemoryManager();

    MemoryAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::localMemory, false, false, MemoryManager::maxOsContextCount);
    allocation.storageInfo.memoryBanks = 0x3u;
    allocation.storageInfo.cloningOfPageTables = false;

    DeviceBitfield deviceBitfield(0x1u);
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), 0, deviceBitfield);
    csr->localMemoryEnabled = true;
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);
    EXPECT_FALSE(csr->isMultiOsContextCapable());

    auto oldCsr = device->getDefaultEngine().commandStreamReceiver;
    device->getDefaultEngine().commandStreamReceiver = csr.get();

    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();

    if (csr->isLocalMemoryEnabled()) {
        auto banksBitfield = memoryOperationsInterface->getMemoryBanksBitfield(&allocation, device.get());
        EXPECT_EQ(0x1lu, banksBitfield.to_ulong());
    }

    device->getDefaultEngine().commandStreamReceiver = oldCsr;
}

TEST_F(AubMemoryOperationsHandlerTests, givenLocalMemoryCloneableAllocationWithManyBanksWhenGetMemoryBanksBitfieldThenAllMemoryBanksAreReturned) {
    auto executionEnvironment = device->getExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hardwareInfo);
    executionEnvironment->initializeMemoryManager();

    MemoryAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::localMemory, false, false, MemoryManager::maxOsContextCount);
    allocation.storageInfo.memoryBanks = 0x3u;
    allocation.storageInfo.cloningOfPageTables = true;

    DeviceBitfield deviceBitfield(1);
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), 0, deviceBitfield);
    csr->localMemoryEnabled = true;
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);
    EXPECT_FALSE(csr->isMultiOsContextCapable());

    auto oldCsr = device->getDefaultEngine().commandStreamReceiver;
    device->getDefaultEngine().commandStreamReceiver = csr.get();

    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();

    if (csr->isLocalMemoryEnabled()) {
        auto banksBitfield = memoryOperationsInterface->getMemoryBanksBitfield(&allocation, device.get());
        EXPECT_EQ(0x3lu, banksBitfield.to_ulong());
    }

    device->getDefaultEngine().commandStreamReceiver = oldCsr;
}

TEST_F(AubMemoryOperationsHandlerTests, givenLocalMemoryNoncloneableAllocationWithManyBanksWhenGetMemoryBanksBitfieldThenAllMemoryBanksAreReturned) {
    auto executionEnvironment = device->getExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hardwareInfo);
    executionEnvironment->initializeMemoryManager();

    MemoryAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::localMemory, false, false, MemoryManager::maxOsContextCount);
    allocation.storageInfo.memoryBanks = 0x3u;
    allocation.storageInfo.cloningOfPageTables = false;

    DeviceBitfield deviceBitfield(0b11);
    const auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), 0, deviceBitfield);
    csr->localMemoryEnabled = true;
    csr->multiOsContextCapable = true;
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);
    EXPECT_TRUE(csr->isMultiOsContextCapable());

    auto oldCsr = device->getDefaultEngine().commandStreamReceiver;
    device->getDefaultEngine().commandStreamReceiver = csr.get();

    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();

    if (csr->isLocalMemoryEnabled()) {
        auto banksBitfield = memoryOperationsInterface->getMemoryBanksBitfield(&allocation, device.get());
        EXPECT_EQ(0x3lu, banksBitfield.to_ulong());
    }

    device->getDefaultEngine().commandStreamReceiver = oldCsr;
}

TEST_F(AubMemoryOperationsHandlerTests, givenGfxAllocationAndNoDeviceWhenSetAubWritableToTrueThenAllocationWritablePropertyIsNotSet) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();

    MemoryAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::localMemory, false, false, MemoryManager::maxOsContextCount);

    memoryOperationsInterface->setAubWritable(true, allocation, device.get());
    ASSERT_TRUE(memoryOperationsInterface->isAubWritable(allocation, device.get()));

    memoryOperationsInterface->setAubWritable(false, allocation, nullptr);
    EXPECT_TRUE(memoryOperationsInterface->isAubWritable(allocation, device.get()));
}

TEST_F(AubMemoryOperationsHandlerTests, givenGfxAllocationAndNoDeviceWhenIsAubWritableWithoutDeviceIsCalledThenReturnFalse) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();

    MemoryAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::localMemory, false, false, MemoryManager::maxOsContextCount);

    memoryOperationsInterface->setAubWritable(true, allocation, device.get());
    ASSERT_TRUE(memoryOperationsInterface->isAubWritable(allocation, device.get()));
    EXPECT_FALSE(memoryOperationsInterface->isAubWritable(allocation, nullptr));
}

TEST_F(AubMemoryOperationsHandlerTests, givenGfxAllocationWhenSetAubWritableToTrueThenAllocationIsAubWritable) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();

    MemoryAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::localMemory, false, false, MemoryManager::maxOsContextCount);

    memoryOperationsInterface->setAubWritable(true, allocation, device.get());
    EXPECT_TRUE(memoryOperationsInterface->isAubWritable(allocation, device.get()));

    memoryOperationsInterface->setAubWritable(false, allocation, device.get());
    EXPECT_FALSE(memoryOperationsInterface->isAubWritable(allocation, device.get()));
}

TEST_F(AubMemoryOperationsHandlerTests, givenGfxAllocationWhenSetAubWritableForNonDefaultBankThenWritablePropertyIsSetCorrectly) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();

    MemoryAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::localMemory, false, false, MemoryManager::maxOsContextCount);

    allocation.storageInfo.cloningOfPageTables = false;

    device->deviceBitfield = 0b1111;

    memoryOperationsInterface->setAubWritable(true, allocation, device.get());
    EXPECT_TRUE(memoryOperationsInterface->isAubWritable(allocation, device.get()));

    device->deviceBitfield = 0b0000;
    memoryOperationsInterface->setAubWritable(false, allocation, device.get());
    EXPECT_FALSE(memoryOperationsInterface->isAubWritable(allocation, device.get()));

    device->deviceBitfield = 0b1000;
    EXPECT_TRUE(memoryOperationsInterface->isAubWritable(allocation, device.get()));
}

HWTEST_F(AubMemoryOperationsHandlerTests, givenCloningOfPageTablesWhenProcessingFlushResidencyThenPerformAUbManagerWriteMemory) {
    auto mockAubManager = std::make_unique<MockAubManager>();
    auto *aubManager = mockAubManager.get();
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    memoryOperationsInterface->setAubManager(aubManager);
    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u));
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hardwareInfo);
    executionEnvironment->initializeMemoryManager();
    auto mockAubCenter = std::make_unique<MockAubCenter>();
    mockAubCenter->aubManager.reset(mockAubManager.release());
    executionEnvironment->rootDeviceEnvironments[0]->aubCenter.reset(mockAubCenter.release());
    auto csr = std::make_unique<MockAubCsr<FamilyType>>("", true, *executionEnvironment, device->getRootDeviceIndex(), device->deviceBitfield);
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);

    allocPtr->storageInfo.cloningOfPageTables = true;

    auto result = memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    EXPECT_EQ(result, MemoryOperationsStatus::success);
    EXPECT_TRUE(aubManager->writeMemory2Called);

    aubManager->writeMemory2Called = false;
    memoryOperationsInterface->processFlushResidency(csr.get());
    EXPECT_TRUE(aubManager->writeMemory2Called);
}

HWTEST_F(AubMemoryOperationsHandlerTests, givenNonLocalGfxAllocationWriteableWhenProcessingFlushResidencyThenPerformAubManagerWriteMemory) {
    auto mockAubManager = std::make_unique<MockAubManager>();
    auto *aubManager = mockAubManager.get();
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    memoryOperationsInterface->setAubManager(aubManager);
    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u));
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hardwareInfo);
    executionEnvironment->initializeMemoryManager();
    auto mockAubCenter = std::make_unique<MockAubCenter>();
    mockAubCenter->aubManager.reset(mockAubManager.release());
    executionEnvironment->rootDeviceEnvironments[0]->aubCenter.reset(mockAubCenter.release());
    auto csr = std::make_unique<MockAubCsr<FamilyType>>("", true, *executionEnvironment, device->getRootDeviceIndex(), device->deviceBitfield);
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);

    MemoryAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::system4KBPages, false, false, MemoryManager::maxOsContextCount);
    allocPtr = &allocation;
    allocPtr->storageInfo.cloningOfPageTables = false;

    auto result = memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    EXPECT_EQ(result, MemoryOperationsStatus::success);
    EXPECT_TRUE(aubManager->writeMemory2Called);

    aubManager->writeMemory2Called = false;

    memoryOperationsInterface->processFlushResidency(csr.get());
    EXPECT_TRUE(aubManager->writeMemory2Called);
}

HWTEST_F(AubMemoryOperationsHandlerTests, givenLocalGfxAllocationWriteableWhenProcessingFlushResidencyThenPerformAllocationDefaultCsrWriteMemory) {
    auto mockAubManager = std::make_unique<MockAubManager>();
    auto *aubManager = mockAubManager.get();
    auto memoryOperationsInterface = getMemoryOperationsHandler();
    memoryOperationsInterface->setAubManager(aubManager);
    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u));
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hardwareInfo);
    executionEnvironment->initializeMemoryManager();
    auto mockAubCenter = std::make_unique<MockAubCenter>();
    mockAubCenter->aubManager.reset(mockAubManager.release());
    executionEnvironment->rootDeviceEnvironments[0]->aubCenter.reset(mockAubCenter.release());
    auto csr = std::make_unique<MockAubCsr<FamilyType>>("", true, *executionEnvironment, device->getRootDeviceIndex(), device->deviceBitfield);
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);
    csr->localMemoryEnabled = true;

    MemoryAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::localMemory, false, false, MemoryManager::maxOsContextCount);
    allocPtr = &allocation;
    allocPtr->storageInfo.cloningOfPageTables = false;

    auto backupDefaultCsr = std::make_unique<VariableBackup<NEO::CommandStreamReceiver *>>(&device->getDefaultEngine().commandStreamReceiver);
    device->getDefaultEngine().commandStreamReceiver = csr.get();

    auto result = memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    EXPECT_EQ(result, MemoryOperationsStatus::success);
    EXPECT_FALSE(aubManager->writeMemory2Called);

    auto mockHwContext0 = static_cast<MockHardwareContext *>(csr->hardwareContextController->hardwareContexts[0].get());

    EXPECT_TRUE(mockHwContext0->writeMemory2Called);

    mockHwContext0->writeMemory2Called = false;
    memoryOperationsInterface->processFlushResidency(csr.get());
    EXPECT_FALSE(aubManager->writeMemory2Called);
    EXPECT_TRUE(mockHwContext0->writeMemory2Called);
}

HWTEST_F(AubMemoryOperationsHandlerTests, givenGfxAllocationWriteableWithGmmPresentWhenProcessingFlushResidencyThenPerformAllocationWriteMemoryAndReflectGmmFlags) {
    auto mockAubManager = std::make_unique<MockAubManager>();
    auto *aubManager = mockAubManager.get();
    getMemoryOperationsHandler()->setAubManager(aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();

    allocPtr->storageInfo.cloningOfPageTables = true;

    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u));
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hardwareInfo);
    executionEnvironment->initializeMemoryManager();

    MockGmm gmm(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper());
    gmm.setCompressionEnabled(true);
    allocPtr->setDefaultGmm(&gmm);

    auto mockAubCenter = std::make_unique<MockAubCenter>();
    mockAubCenter->aubManager.reset(mockAubManager.release());

    executionEnvironment->rootDeviceEnvironments[0]->aubCenter.reset(mockAubCenter.release());

    auto result = memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    EXPECT_EQ(result, MemoryOperationsStatus::success);

    aubManager->writeMemory2Called = false;
    aubManager->storeAllocationParams = true;

    auto csr = std::make_unique<MockAubCsr<FamilyType>>("", true, *executionEnvironment, device->getRootDeviceIndex(), device->deviceBitfield);
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor());
    csr->setupContext(*osContext);

    memoryOperationsInterface->processFlushResidency(csr.get());

    EXPECT_TRUE(aubManager->writeMemory2Called);
    EXPECT_EQ(1u, aubManager->storedAllocationParams.size());
    EXPECT_TRUE(aubManager->storedAllocationParams[0].additionalParams.compressionEnabled);
    EXPECT_FALSE(aubManager->storedAllocationParams[0].additionalParams.uncached);
}

TEST_F(AubMemoryOperationsHandlerTests, givenDeviceNullWhenProcessingFlushResidencyThenAllocationIsNotWriteable) {
    MockAubManager aubManager;
    getMemoryOperationsHandler()->setAubManager(&aubManager);
    auto memoryOperationsInterface = getMemoryOperationsHandler();

    MemoryAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, reinterpret_cast<void *>(0x1000), 0x1000u,
                                MemoryConstants::pageSize, 0, MemoryPool::localMemory, false, false, MemoryManager::maxOsContextCount);

    allocation.storageInfo.cloningOfPageTables = false;

    device->deviceBitfield = 0b1111;

    memoryOperationsInterface->setAubWritable(true, allocation, device.get());
    EXPECT_TRUE(memoryOperationsInterface->isAubWritable(allocation, device.get()));

    auto result = memoryOperationsInterface->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&allocPtr, 1), false);
    EXPECT_EQ(result, MemoryOperationsStatus::success);
    EXPECT_TRUE(aubManager.writeMemory2Called);

    aubManager.writeMemory2Called = false;

    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), 0, device->deviceBitfield);

    memoryOperationsInterface->processFlushResidency(csr.get());
    EXPECT_FALSE(aubManager.writeMemory2Called);
}

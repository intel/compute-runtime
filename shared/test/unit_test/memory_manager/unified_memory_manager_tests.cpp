/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(SvmDeviceAllocationTest, givenGivenSvmAllocsManagerWhenObtainOwnershipCalledThenLockedUniqueLockReturned) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);

    auto lock = svmManager->obtainOwnership();
    std::thread th1([&] {
        EXPECT_FALSE(svmManager->mtxForIndirectAccess.try_lock());
    });
    th1.join();
    lock.unlock();
    std::thread th2([&] {
        EXPECT_TRUE(svmManager->mtxForIndirectAccess.try_lock());
        svmManager->mtxForIndirectAccess.unlock();
    });
    th2.join();
}

using SVMLocalMemoryAllocatorTest = Test<SVMMemoryAllocatorFixture<true>>;
TEST_F(SVMLocalMemoryAllocatorTest, whenFreeSharedAllocWithOffsetPointerThenResourceIsRemovedProperly) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableLocalMemory.set(1);
    void *cmdQ = reinterpret_cast<void *>(0x12345);
    auto mockPageFaultManager = new MockPageFaultManager();
    memoryManager->pageFaultManager.reset(mockPageFaultManager);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY, rootDeviceIndices, deviceBitfields);
    auto allocationSize = 4096u;
    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties, &cmdQ);
    EXPECT_NE(nullptr, ptr);
    auto svmData = svmManager->getSVMAlloc(ptr);
    auto pageFaultMemoryData = mockPageFaultManager->memoryData.find(ptr);
    EXPECT_NE(svmData, nullptr);
    EXPECT_NE(pageFaultMemoryData, mockPageFaultManager->memoryData.end());

    svmManager->freeSVMAlloc(ptrOffset(ptr, allocationSize / 4));

    svmData = svmManager->getSVMAlloc(ptr);
    pageFaultMemoryData = mockPageFaultManager->memoryData.find(ptr);
    EXPECT_EQ(svmData, nullptr);
    EXPECT_EQ(pageFaultMemoryData, mockPageFaultManager->memoryData.end());
}

TEST_F(SVMLocalMemoryAllocatorTest, whenFreeSVMAllocIsDeferredThenFreedSubsequently) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableLocalMemory.set(1);

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);
    void *cmdQ = reinterpret_cast<void *>(0x12345);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;

    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096, unifiedMemoryProperties, &cmdQ);
    EXPECT_NE(nullptr, ptr);

    auto memoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    memoryManager->deferAllocInUse = true;
    svmManager->freeSVMAllocDefer(ptr);
    memoryManager->deferAllocInUse = false;
    svmManager->freeSVMAllocDefer(ptr);
    ASSERT_EQ(svmManager->getSVMAlloc(ptr), nullptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenMultipleFreeSVMAllocDeferredThenFreedSubsequently) {

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;

    auto ptr = svmManager->createUnifiedMemoryAllocation(4096, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr);
    auto ptr1 = svmManager->createUnifiedMemoryAllocation(4096, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr1);
    auto ptr2 = svmManager->createUnifiedMemoryAllocation(4096, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr2);

    auto memoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    memoryManager->deferAllocInUse = true;
    svmManager->freeSVMAllocDefer(ptr);
    ASSERT_NE(svmManager->getSVMAlloc(ptr), nullptr);
    EXPECT_EQ(1ul, svmManager->getNumDeferFreeAllocs());
    svmManager->freeSVMAllocDefer(ptr1);
    ASSERT_NE(svmManager->getSVMAlloc(ptr1), nullptr);
    EXPECT_EQ(2ul, svmManager->getNumDeferFreeAllocs());
    memoryManager->deferAllocInUse = false;
    svmManager->freeSVMAlloc(ptr2, true);
    EXPECT_EQ(0ul, svmManager->getNumDeferFreeAllocs());
    ASSERT_EQ(svmManager->getSVMAlloc(ptr), nullptr);
    ASSERT_EQ(svmManager->getSVMAlloc(ptr1), nullptr);
    ASSERT_EQ(svmManager->getSVMAlloc(ptr2), nullptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenPointerWithOffsetPassedThenProperDataRetrieved) {

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;

    auto ptr = svmManager->createUnifiedMemoryAllocation(4096, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr);

    auto offsetedPointer = ptrOffset(ptr, 4u);
    auto usmAllocationData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, usmAllocationData);
    auto usmAllocationData2 = svmManager->getSVMAlloc(offsetedPointer);
    ASSERT_NE(nullptr, usmAllocationData2);
    EXPECT_EQ(usmAllocationData2, usmAllocationData);
    svmManager->freeSVMAlloc(ptr, true);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenMultiplePointerWithOffsetPassedThenProperDataRetrieved) {

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;

    auto ptr = svmManager->createUnifiedMemoryAllocation(2048, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr);
    auto ptr2 = svmManager->createUnifiedMemoryAllocation(2048, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr);

    void *unalignedPointer = reinterpret_cast<void *>(0x1234);
    MockGraphicsAllocation mockAllocation(unalignedPointer, 512u);
    SvmAllocationData allocationData(1u);

    allocationData.memoryType = InternalMemoryType::HOST_UNIFIED_MEMORY;
    allocationData.size = mockAllocation.getUnderlyingBufferSize();
    allocationData.gpuAllocations.addAllocation(&mockAllocation);
    svmManager->SVMAllocs.insert(allocationData);

    auto offsetedPointer = ptrOffset(ptr, 2048);
    auto usmAllocationData = svmManager->getSVMAlloc(offsetedPointer);
    EXPECT_EQ(nullptr, usmAllocationData);
    offsetedPointer = ptrOffset(ptr2, 2048);
    usmAllocationData = svmManager->getSVMAlloc(offsetedPointer);
    EXPECT_EQ(nullptr, usmAllocationData);
    usmAllocationData = svmManager->getSVMAlloc(unalignedPointer);
    EXPECT_NE(nullptr, usmAllocationData);

    svmManager->SVMAllocs.remove(allocationData);
    svmManager->freeSVMAlloc(ptr, true);
    svmManager->freeSVMAlloc(ptr2, true);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenKmdMigratedSharedAllocationWhenPrefetchMemoryIsCalledForMultipleActivePartitionsThenPrefetchAllocationToSubDevices) {
    DebugManagerStateRestore restore;
    DebugManager.flags.UseKmdMigration.set(1);

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);
    void *cmdQ = reinterpret_cast<void *>(0x12345);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096, unifiedMemoryProperties, &cmdQ);
    EXPECT_NE(nullptr, ptr);

    auto svmData = svmManager->getSVMAlloc(ptr);

    csr->setActivePartitions(2);
    svmManager->prefetchMemory(*device, *csr, *svmData);

    auto mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    EXPECT_TRUE(mockMemoryManager->setMemPrefetchCalled);
    EXPECT_EQ(2u, mockMemoryManager->memPrefetchSubDeviceIds.size());

    for (auto index = 0u; index < mockMemoryManager->memPrefetchSubDeviceIds.size(); index++) {
        EXPECT_EQ(index, mockMemoryManager->memPrefetchSubDeviceIds[index]);
    }

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenForceMemoryPrefetchForKmdMigratedSharedAllocationsWhenSVMAllocsIsCalledThenPrefetchSharedUnifiedMemoryInSvmAllocsManager) {
    DebugManagerStateRestore restore;
    DebugManager.flags.UseKmdMigration.set(1);
    DebugManager.flags.ForceMemoryPrefetchForKmdMigratedSharedAllocations.set(true);

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);
    void *cmdQ = reinterpret_cast<void *>(0x12345);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096, unifiedMemoryProperties, &cmdQ);
    EXPECT_NE(nullptr, ptr);

    svmManager->prefetchSVMAllocs(*device, *csr);

    auto mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    EXPECT_TRUE(mockMemoryManager->setMemPrefetchCalled);
    EXPECT_EQ(1u, mockMemoryManager->memPrefetchSubDeviceIds.size());

    svmManager->freeSVMAlloc(ptr);
}

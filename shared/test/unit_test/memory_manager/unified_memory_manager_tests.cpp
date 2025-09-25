/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/unified_memory/usm_memory_support.h"
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
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());

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
    debugManager.flags.EnableLocalMemory.set(1);
    void *cmdQ = reinterpret_cast<void *>(0x12345);
    auto mockPageFaultManager = new MockPageFaultManager();
    memoryManager->pageFaultManager.reset(mockPageFaultManager);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
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

TEST_F(SVMLocalMemoryAllocatorTest, whenFreeSvmAllocationDeferThenAllocationsCountIsProper) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableLocalMemory.set(1);

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);
    void *cmdQ = reinterpret_cast<void *>(0x12345);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;

    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096, unifiedMemoryProperties, &cmdQ);
    EXPECT_NE(nullptr, ptr);

    auto memoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    memoryManager->deferAllocInUse = true;
    EXPECT_EQ(svmManager->svmDeferFreeAllocs.allocations.size(), 0u);
    svmManager->freeSVMAllocDefer(ptr);
    EXPECT_EQ(svmManager->svmDeferFreeAllocs.allocations.size(), 1u);
    svmManager->freeSVMAllocDefer(ptr);
    EXPECT_EQ(svmManager->svmDeferFreeAllocs.allocations.size(), 1u);
    memoryManager->deferAllocInUse = false;
    svmManager->freeSVMAllocDefer(ptr);
    EXPECT_EQ(svmManager->svmDeferFreeAllocs.allocations.size(), 0u);
    ASSERT_EQ(svmManager->getSVMAlloc(ptr), nullptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenFreeSVMAllocIsDeferredThenFreedSubsequently) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableLocalMemory.set(1);

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);
    void *cmdQ = reinterpret_cast<void *>(0x12345);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
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

TEST_F(SVMLocalMemoryAllocatorTest, GivenTwoRootDevicesWhenAllocatingSharedMemoryForDevice2ThenAllocationsHappenForRootDeviceIndexOneAndNotZero) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableLocalMemory.set(1);

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(2, 0));
    auto device = deviceFactory->rootDevices[1];
    std::map<uint32_t, NEO::DeviceBitfield> deviceBitfieldsLocal;
    deviceBitfieldsLocal[1] = device->getDeviceBitfield();
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);
    void *cmdQ = reinterpret_cast<void *>(0x12345);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfieldsLocal);
    unifiedMemoryProperties.device = device;

    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096, unifiedMemoryProperties, &cmdQ);
    EXPECT_NE(nullptr, ptr);

    EXPECT_EQ(svmManager->getNumAllocs(), 1u);
    EXPECT_EQ(svmManager->getSVMAlloc(ptr)->gpuAllocations.getGraphicsAllocation(0), nullptr);
    EXPECT_NE(svmManager->getSVMAlloc(ptr)->gpuAllocations.getGraphicsAllocation(1), nullptr);
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
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
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
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;

    auto ptr = svmManager->createUnifiedMemoryAllocation(4096, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr);

    auto offsetPointer = ptrOffset(ptr, 4u);
    auto usmAllocationData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, usmAllocationData);
    auto usmAllocationData2 = svmManager->getSVMAlloc(offsetPointer);
    ASSERT_NE(nullptr, usmAllocationData2);
    EXPECT_EQ(usmAllocationData2, usmAllocationData);
    svmManager->freeSVMAlloc(ptr, true);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenMultiplePointerWithOffsetPassedThenProperDataRetrieved) {

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;

    auto ptr = svmManager->createUnifiedMemoryAllocation(2048, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr);
    auto ptr2 = svmManager->createUnifiedMemoryAllocation(2048, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr);

    void *unalignedPointer = reinterpret_cast<void *>(0x1234);
    MockGraphicsAllocation mockAllocation(unalignedPointer, 512u);
    SvmAllocationData allocationData(1u);

    allocationData.memoryType = InternalMemoryType::hostUnifiedMemory;
    allocationData.size = mockAllocation.getUnderlyingBufferSize();
    allocationData.gpuAllocations.addAllocation(&mockAllocation);
    svmManager->svmAllocs.insert(unalignedPointer, allocationData);

    auto offsetPointer = ptrOffset(ptr, 2048);
    auto usmAllocationData = svmManager->getSVMAlloc(offsetPointer);
    EXPECT_EQ(nullptr, usmAllocationData);
    offsetPointer = ptrOffset(ptr2, 2048);
    usmAllocationData = svmManager->getSVMAlloc(offsetPointer);
    EXPECT_EQ(nullptr, usmAllocationData);
    usmAllocationData = svmManager->getSVMAlloc(unalignedPointer);
    EXPECT_NE(nullptr, usmAllocationData);

    svmManager->svmAllocs.remove(reinterpret_cast<void *>(allocationData.gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()));
    svmManager->freeSVMAlloc(ptr, true);
    svmManager->freeSVMAlloc(ptr2, true);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenKmdMigratedSharedAllocationWhenPrefetchMemoryIsCalledForMultipleActivePartitionsThenPrefetchAllocationToSubDevices) {
    DebugManagerStateRestore restore;
    debugManager.flags.UseKmdMigration.set(1);

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);
    void *cmdQ = reinterpret_cast<void *>(0x12345);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096, unifiedMemoryProperties, &cmdQ);
    EXPECT_NE(nullptr, ptr);

    csr->setActivePartitions(2);
    svmManager->prefetchMemory(*device, *csr, ptr, 4096);

    auto mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    EXPECT_TRUE(mockMemoryManager->setMemPrefetchCalled);
    EXPECT_EQ(2u, mockMemoryManager->memPrefetchSubDeviceIds.size());

    for (auto index = 0u; index < mockMemoryManager->memPrefetchSubDeviceIds.size(); index++) {
        EXPECT_EQ(index, mockMemoryManager->memPrefetchSubDeviceIds[index]);
    }

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenNonKmdMigratedSharedAllocationWhenPrefetchMemoryIsCalledThenSetMemPrefetchNotCalled) {
    DebugManagerStateRestore restore;
    debugManager.flags.UseKmdMigration.set(0);

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);
    void *cmdQ = reinterpret_cast<void *>(0x12345);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096, unifiedMemoryProperties, &cmdQ);
    EXPECT_NE(nullptr, ptr);

    csr->setActivePartitions(2);
    svmManager->prefetchMemory(*device, *csr, ptr, 4096);

    auto mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    EXPECT_FALSE(mockMemoryManager->setMemPrefetchCalled);
    EXPECT_EQ(0u, mockMemoryManager->memPrefetchSubDeviceIds.size());

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenNonSharedUnifiedMemoryAllocationWhenPrefetchMemoryIsCalledThenSetMemPrefetchNotCalled) {
    DebugManagerStateRestore restore;
    debugManager.flags.UseKmdMigration.set(1);

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);
    void *cmdQ = reinterpret_cast<void *>(0x12345);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096, unifiedMemoryProperties, &cmdQ);
    EXPECT_NE(nullptr, ptr);

    auto svmData = svmManager->getSVMAlloc(ptr);
    svmData->memoryType = InternalMemoryType::deviceUnifiedMemory;

    csr->setActivePartitions(2);
    svmManager->prefetchMemory(*device, *csr, ptr, 4096);

    auto mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    EXPECT_FALSE(mockMemoryManager->setMemPrefetchCalled);
    EXPECT_EQ(0u, mockMemoryManager->memPrefetchSubDeviceIds.size());

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenSharedSystemAllocationWhenPrefetchMemoryIsCalledThenPrefetchAllocationToSystemMemory) {
    DebugManagerStateRestore restore;

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    auto &hwInfo = *device->getRootDeviceEnvironment().getMutableHardwareInfo();

    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);

    csr->setupContext(*device->getDefaultEngine().osContext);

    debugManager.flags.EnableSharedSystemUsmSupport.set(1);

    auto ptr = malloc(4096);
    EXPECT_NE(nullptr, ptr);

    svmManager->prefetchMemory(*device, *csr, ptr, 4096);

    auto mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    EXPECT_TRUE(mockMemoryManager->prefetchSharedSystemAllocCalled);

    free(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenSharedSystemAllocationWhenPrefetchMemoryIsCalledAndNotEnabledThenNoPrefetchAllocationToSystemMemory) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(0);

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);

    auto ptr = malloc(4096);
    EXPECT_NE(nullptr, ptr);

    svmManager->prefetchMemory(*device, *csr, ptr, 4096);

    auto mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    EXPECT_FALSE(mockMemoryManager->prefetchSharedSystemAllocCalled);

    free(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenSharedSystemAllocationWhenSharedSystemMemAdviseIsCalledThenMemoryManagerSetSharedSystemMemAdviseIsCalled) {

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());

    MemAdvise memAdviseOp = MemAdvise::setSystemMemoryPreferredLocation;
    auto ptr = malloc(4096);
    EXPECT_NE(nullptr, ptr);

    svmManager->sharedSystemMemAdvise(*device, memAdviseOp, ptr, 4096);

    auto mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    EXPECT_TRUE(mockMemoryManager->setSharedSystemMemAdviseCalled);

    free(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenSharedSystemAllocationWhenSharedSystemAtomicAccessIsCalledThenMemoryManagerSetSharedSystemAtomicAccessIsCalled) {

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());

    AtomicAccessMode mode = AtomicAccessMode::device;
    auto ptr = malloc(4096);
    EXPECT_NE(nullptr, ptr);

    svmManager->sharedSystemAtomicAccess(*device, mode, ptr, 4096);

    auto mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    EXPECT_TRUE(mockMemoryManager->setSharedSystemAtomicAccessCalled);

    free(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenSharedSystemAllocationWhenGetSharedSystemAtomicAccessIsCalledThenMemoryManagerGetSharedSystemAtomicAccessIsCalledAndFails) {

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    auto mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    mockMemoryManager->failGetSharedSystemAtomicAccess = true;

    auto ptr = malloc(4096);
    EXPECT_NE(nullptr, ptr);
    auto ret = svmManager->getSharedSystemAtomicAccess(*device, ptr, 4096);
    EXPECT_EQ(AtomicAccessMode::invalid, ret);

    EXPECT_TRUE(mockMemoryManager->getSharedSystemAtomicAccessCalled);

    free(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenForceMemoryPrefetchForKmdMigratedSharedAllocationsWhenSVMAllocsIsCalledThenPrefetchSharedUnifiedMemoryInSvmAllocsManager) {
    DebugManagerStateRestore restore;
    debugManager.flags.UseKmdMigration.set(1);
    debugManager.flags.ForceMemoryPrefetchForKmdMigratedSharedAllocations.set(true);

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);
    void *cmdQ = reinterpret_cast<void *>(0x12345);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096, unifiedMemoryProperties, &cmdQ);
    EXPECT_NE(nullptr, ptr);

    svmManager->prefetchSVMAllocs(*device, *csr);

    auto mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    EXPECT_TRUE(mockMemoryManager->setMemPrefetchCalled);
    EXPECT_EQ(1u, mockMemoryManager->memPrefetchSubDeviceIds.size());

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenAlignmentThenUnifiedMemoryAllocationsAreAlignedCorrectly) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);

    size_t alignment = 8 * MemoryConstants::megaByte;
    do {
        alignment >>= 1;
        memoryManager->validateAllocateProperties = [alignment](const AllocationProperties &properties) {
            EXPECT_EQ(properties.alignment, alignUpNonZero<size_t>(alignment, MemoryConstants::pageSize64k));
        };
        SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, alignment, rootDeviceIndices, deviceBitfields);
        unifiedMemoryProperties.device = device;
        auto ptr = svmManager->createUnifiedMemoryAllocation(1, unifiedMemoryProperties);
        EXPECT_NE(nullptr, ptr);
        if (alignment != 0) {
            EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) & (~(alignment - 1)), reinterpret_cast<uintptr_t>(ptr));
        }
        EXPECT_EQ(svmManager->getSVMAlloc(ptr)->pageSizeForAlignment, MemoryConstants::pageSize64k);
        svmManager->freeSVMAlloc(ptr);
    } while (alignment != 0);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenAlignmentThenHostUnifiedMemoryAllocationsAreAlignedCorrectly) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);

    size_t alignment = 8 * MemoryConstants::megaByte;
    do {
        alignment >>= 1;
        memoryManager->validateAllocateProperties = [alignment](const AllocationProperties &properties) {
            EXPECT_EQ(properties.alignment, alignUpNonZero<size_t>(alignment, MemoryConstants::pageSize));
        };
        SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, alignment, rootDeviceIndices, deviceBitfields);
        unifiedMemoryProperties.device = device;
        auto ptr = svmManager->createHostUnifiedMemoryAllocation(1, unifiedMemoryProperties);
        EXPECT_NE(nullptr, ptr);
        if (alignment != 0) {
            EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) & (~(alignment - 1)), reinterpret_cast<uintptr_t>(ptr));
        }
        EXPECT_EQ(svmManager->getSVMAlloc(ptr)->pageSizeForAlignment, MemoryConstants::pageSize);
        svmManager->freeSVMAlloc(ptr);
    } while (alignment != 0);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenUncachedHostAllocationThenSetAllocationAsUncached) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 0ull, rootDeviceIndices, deviceBitfields);

    unifiedMemoryProperties.allocationFlags.flags.locallyUncachedResource = true;
    auto ptr = svmManager->createHostUnifiedMemoryAllocation(1, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr);
    auto mockedAllocation = static_cast<MemoryAllocation *>(svmManager->getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation());
    EXPECT_TRUE(mockedAllocation->uncacheable);

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenAlignmentThenSharedUnifiedMemoryAllocationsAreAlignedCorrectly) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);

    void *cmdQ = reinterpret_cast<void *>(0x12345);
    auto mockPageFaultManager = new MockPageFaultManager();
    memoryManager->pageFaultManager.reset(mockPageFaultManager);

    size_t alignment = 8 * MemoryConstants::megaByte;
    do {
        alignment >>= 1;
        memoryManager->validateAllocateProperties = [alignment](const AllocationProperties &properties) {
            EXPECT_EQ(properties.alignment, alignUpNonZero<size_t>(alignment, MemoryConstants::pageSize64k));
        };
        SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, alignment, rootDeviceIndices, deviceBitfields);
        unifiedMemoryProperties.device = device;
        auto ptr = svmManager->createSharedUnifiedMemoryAllocation(1, unifiedMemoryProperties, cmdQ);
        EXPECT_NE(nullptr, ptr);
        if (alignment != 0) {
            EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) & (~(alignment - 1)), reinterpret_cast<uintptr_t>(ptr));
        }
        EXPECT_EQ(svmManager->getSVMAlloc(ptr)->pageSizeForAlignment, MemoryConstants::pageSize64k);
        svmManager->freeSVMAlloc(ptr);
    } while (alignment != 0);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenAlignmentWhenLocalMemoryIsEnabledThenSharedUnifiedMemoryAllocationsAreAlignedCorrectly) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableLocalMemory.set(1);

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);

    void *cmdQ = reinterpret_cast<void *>(0x12345);
    auto mockPageFaultManager = new MockPageFaultManager();
    memoryManager->pageFaultManager.reset(mockPageFaultManager);

    const size_t svmCpuAlignment = memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[0]->getProductHelper().getSvmCpuAlignment();
    size_t alignment = 8 * MemoryConstants::megaByte;
    do {
        alignment >>= 1;
        memoryManager->validateAllocateProperties = [alignment, svmCpuAlignment](const AllocationProperties &properties) {
            EXPECT_EQ(properties.alignment, alignUpNonZero<size_t>(alignment, std::max(MemoryConstants::pageSize64k, svmCpuAlignment)));
        };
        SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, alignment, rootDeviceIndices, deviceBitfields);
        unifiedMemoryProperties.device = device;
        auto ptr = svmManager->createSharedUnifiedMemoryAllocation(1, unifiedMemoryProperties, cmdQ);
        EXPECT_NE(nullptr, ptr);
        if (alignment != 0) {
            EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) & (~(alignment - 1)), reinterpret_cast<uintptr_t>(ptr));
        }
        const size_t pageSizeForAlignment = std::max(MemoryConstants::pageSize64k, svmCpuAlignment);
        EXPECT_EQ(svmManager->getSVMAlloc(ptr)->pageSizeForAlignment, pageSizeForAlignment);
        svmManager->freeSVMAlloc(ptr);
    } while (alignment != 0);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenInternalAllocationWhenItIsMadeResidentThenNewTrackingEntryIsCreated) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);

    void *cmdQ = reinterpret_cast<void *>(0x12345);
    auto mockPageFaultManager = new MockPageFaultManager();
    memoryManager->pageFaultManager.reset(mockPageFaultManager);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, &cmdQ);

    ASSERT_NE(nullptr, ptr);
    auto graphicsAllocation = svmManager->getSVMAlloc(ptr);
    EXPECT_FALSE(graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->isResident(csr->getOsContext().getContextId()));

    EXPECT_EQ(0u, svmManager->indirectAllocationsResidency.size());

    EXPECT_TRUE(svmManager->submitIndirectAllocationsAsPack(*csr));
    EXPECT_TRUE(graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->isResident(csr->getOsContext().getContextId()));
    EXPECT_EQ(GraphicsAllocation::objectAlwaysResident, graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->getResidencyTaskCount(csr->getOsContext().getContextId()));
    EXPECT_FALSE(graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->peekEvictable());

    EXPECT_EQ(1u, svmManager->indirectAllocationsResidency.size());
    auto internalEntry = svmManager->indirectAllocationsResidency.find(csr.get())->second;
    EXPECT_EQ(1u, internalEntry.latestSentTaskCount);
    EXPECT_EQ(1u, internalEntry.latestResidentObjectId);

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenSubmitIndirectAllocationsAsPackCalledButAllocationsAsPackNotAllowedThenDontMakeResident) {
    DebugManagerStateRestore restore;
    debugManager.flags.MakeIndirectAllocationsResidentAsPack.set(0);
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);

    void *cmdQ = reinterpret_cast<void *>(0x12345);
    auto mockPageFaultManager = new MockPageFaultManager();
    memoryManager->pageFaultManager.reset(mockPageFaultManager);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, &cmdQ);

    ASSERT_NE(nullptr, ptr);
    auto graphicsAllocation = svmManager->getSVMAlloc(ptr);

    EXPECT_FALSE(graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->isResident(csr->getOsContext().getContextId()));
    EXPECT_EQ(0u, svmManager->indirectAllocationsResidency.size());

    EXPECT_FALSE(svmManager->submitIndirectAllocationsAsPack(*csr));

    EXPECT_FALSE(graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->isResident(csr->getOsContext().getContextId()));
    EXPECT_EQ(0u, svmManager->indirectAllocationsResidency.size());
    EXPECT_EQ(svmManager->indirectAllocationsResidency.find(csr.get()), svmManager->indirectAllocationsResidency.end());

    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenInternalAllocationWhenItIsMadeResidentThenSubsequentCallsDoNotCallResidency) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);

    void *cmdQ = reinterpret_cast<void *>(0x12345);
    auto mockPageFaultManager = new MockPageFaultManager();
    memoryManager->pageFaultManager.reset(mockPageFaultManager);
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, &cmdQ);
    ASSERT_NE(nullptr, ptr);

    auto graphicsAllocation = svmManager->getSVMAlloc(ptr);
    svmManager->makeIndirectAllocationsResident(*csr, 1u);

    EXPECT_TRUE(graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->isResident(csr->getOsContext().getContextId()));

    // now call with task count 2 , allocations shouldn't change
    svmManager->makeIndirectAllocationsResident(*csr, 2u);
    auto internalEntry = svmManager->indirectAllocationsResidency.find(csr.get())->second;

    EXPECT_EQ(2u, internalEntry.latestSentTaskCount);
    EXPECT_TRUE(graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->isResident(csr->getOsContext().getContextId()));

    // force Graphics Allocation to be non resident
    graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->updateResidencyTaskCount(GraphicsAllocation::objectNotResident, csr->getOsContext().getContextId());
    EXPECT_FALSE(graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->isResident(csr->getOsContext().getContextId()));

    // now call with task count 3 , allocations shouldn't change
    svmManager->makeIndirectAllocationsResident(*csr, 2u);
    EXPECT_FALSE(graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->isResident(csr->getOsContext().getContextId()));
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenInternalAllocationWhenNewAllocationIsCreatedThenItIsMadeResident) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    csr->setupContext(*device->getDefaultEngine().osContext);

    void *cmdQ = reinterpret_cast<void *>(0x12345);
    auto mockPageFaultManager = new MockPageFaultManager();
    memoryManager->pageFaultManager.reset(mockPageFaultManager);
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, &cmdQ);
    ASSERT_NE(nullptr, ptr);

    auto graphicsAllocation = svmManager->getSVMAlloc(ptr);
    svmManager->makeIndirectAllocationsResident(*csr, 1u);

    EXPECT_TRUE(graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->isResident(csr->getOsContext().getContextId()));
    // force to non resident
    graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->updateResidencyTaskCount(GraphicsAllocation::objectNotResident, csr->getOsContext().getContextId());

    auto ptr2 = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, &cmdQ);
    auto graphicsAllocation2 = svmManager->getSVMAlloc(ptr2);

    EXPECT_FALSE(graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->isResident(csr->getOsContext().getContextId()));

    EXPECT_FALSE(graphicsAllocation2->gpuAllocations.getDefaultGraphicsAllocation()->isResident(csr->getOsContext().getContextId()));

    // now call with task count 2, first allocation shouldn't be modified
    svmManager->makeIndirectAllocationsResident(*csr, 2u);

    EXPECT_FALSE(graphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->isResident(csr->getOsContext().getContextId()));
    EXPECT_TRUE(graphicsAllocation2->gpuAllocations.getDefaultGraphicsAllocation()->isResident(csr->getOsContext().getContextId()));

    svmManager->freeSVMAlloc(ptr);
    svmManager->freeSVMAlloc(ptr2);
}

TEST_F(SVMLocalMemoryAllocatorTest, givenLocalMemoryEnabledAndCompressionEnabledThenDeviceSideSharedUsmIsCompressed) {
    DebugManagerStateRestore restore;
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];

    auto &hwInfo = device->getHardwareInfo();
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    if (!gfxCoreHelper.usmCompressionSupported(hwInfo)) {
        GTEST_SKIP();
    }

    void *cmdQ = reinterpret_cast<void *>(0x12345);
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096, unifiedMemoryProperties, &cmdQ);
    EXPECT_NE(nullptr, ptr);
    auto svmData = svmManager->getSVMAlloc(ptr);
    EXPECT_TRUE(svmData->gpuAllocations.getDefaultGraphicsAllocation()->isCompressionEnabled());

    svmManager->freeSVMAlloc(ptr);
}
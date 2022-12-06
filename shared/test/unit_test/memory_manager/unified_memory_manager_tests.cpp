/*
 * Copyright (C) 2022 Intel Corporation
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

TEST_F(SVMLocalMemoryAllocatorTest, whenCreateUnifiedMemoryAllocationWithSmallSizeThenSetLockable) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceLocalMemoryAccessMode.set(0);
    DebugManager.flags.EnableLocalMemory.set(1);
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    auto ptr = svmManager->createUnifiedMemoryAllocation(4096u, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr);
    EXPECT_TRUE(svmManager->getSVMAlloc(ptr)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex)->storageInfo.isLockable);
    svmManager->freeSVMAlloc(ptr);
}

TEST_F(SVMLocalMemoryAllocatorTest, whenCreateUnifiedMemoryAllocationWithLargeSizeThenSetLockable) {
    if (HwInfoConfig::get(defaultHwInfo->platform.eProductFamily)->isStorageInfoAdjustmentRequired()) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceLocalMemoryAccessMode.set(0);
    DebugManager.flags.EnableLocalMemory.set(1);
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    size_t largeSize = std::max(NonUsmCpuCopyConstants::d2HThreshold, NonUsmCpuCopyConstants::h2DThreshold) + 1;
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    auto ptr = svmManager->createUnifiedMemoryAllocation(largeSize, unifiedMemoryProperties);
    EXPECT_NE(nullptr, ptr);

    EXPECT_FALSE(svmManager->getSVMAlloc(ptr)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex)->storageInfo.isLockable);
    svmManager->freeSVMAlloc(ptr);
}
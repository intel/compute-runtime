/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
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
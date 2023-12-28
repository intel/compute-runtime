/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_pooling.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

using namespace NEO;

using UnifiedMemoryPoolingTest = Test<SVMMemoryAllocatorFixture<true>>;
TEST_F(UnifiedMemoryPoolingTest, givenUsmAllocPoolWhenCallingIsInitializedThenReturnCorrectValue) {
    UsmMemAllocPool usmMemAllocPool;
    EXPECT_FALSE(usmMemAllocPool.isInitialized());

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize2M, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;

    EXPECT_TRUE(usmMemAllocPool.initialize(svmManager.get(), unifiedMemoryProperties, 1 * MemoryConstants::megaByte));
    EXPECT_TRUE(usmMemAllocPool.isInitialized());

    usmMemAllocPool.cleanup();
    EXPECT_FALSE(usmMemAllocPool.isInitialized());
    EXPECT_FALSE(usmMemAllocPool.freeSVMAlloc(reinterpret_cast<void *>(0x1), true));
}

template <InternalMemoryType poolMemoryType, bool failAllocation>
class InitializedUnifiedMemoryPoolingTest : public UnifiedMemoryPoolingTest {
  public:
    void SetUp() override {
        UnifiedMemoryPoolingTest::setUp();
        EXPECT_FALSE(usmMemAllocPool.isInitialized());

        deviceFactory = std::unique_ptr<UltDeviceFactory>(new UltDeviceFactory(1, 1));
        device = deviceFactory->rootDevices[0];
        svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
        static_cast<MockMemoryManager *>(device->getMemoryManager())->failInDevicePoolWithError = failAllocation;

        poolMemoryProperties = std::make_unique<SVMAllocsManager::UnifiedMemoryProperties>(poolMemoryType, MemoryConstants::pageSize2M, rootDeviceIndices, deviceBitfields);
        poolMemoryProperties->device = device;
        ASSERT_EQ(!failAllocation, usmMemAllocPool.initialize(svmManager.get(), *poolMemoryProperties.get(), poolSize));
    }
    void TearDown() override {
        usmMemAllocPool.cleanup();
        UnifiedMemoryPoolingTest::tearDown();
    }

    const size_t poolSize = 2 * MemoryConstants::megaByte;
    MockUsmMemAllocPool usmMemAllocPool;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
    Device *device;
    std::unique_ptr<MockSVMAllocsManager> svmManager;
    std::unique_ptr<SVMAllocsManager::UnifiedMemoryProperties> poolMemoryProperties;
};

using InitializedHostUnifiedMemoryPoolingTest = InitializedUnifiedMemoryPoolingTest<InternalMemoryType::hostUnifiedMemory, false>;
TEST_F(InitializedHostUnifiedMemoryPoolingTest, givenDifferentAllocationSizesWhenCallingCanBePooledThenCorrectValueIsReturned) {
    SVMAllocsManager::UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize64k, rootDeviceIndices, deviceBitfields);
    EXPECT_TRUE(usmMemAllocPool.canBePooled(UsmMemAllocPool::allocationThreshold, memoryProperties));
    EXPECT_FALSE(usmMemAllocPool.canBePooled(UsmMemAllocPool::allocationThreshold + 1, memoryProperties));

    memoryProperties.memoryType = InternalMemoryType::deviceUnifiedMemory;
    EXPECT_FALSE(usmMemAllocPool.canBePooled(UsmMemAllocPool::allocationThreshold, memoryProperties));
    EXPECT_FALSE(usmMemAllocPool.canBePooled(UsmMemAllocPool::allocationThreshold + 1, memoryProperties));

    memoryProperties.memoryType = InternalMemoryType::sharedUnifiedMemory;
    EXPECT_FALSE(usmMemAllocPool.canBePooled(UsmMemAllocPool::allocationThreshold, memoryProperties));
    EXPECT_FALSE(usmMemAllocPool.canBePooled(UsmMemAllocPool::allocationThreshold + 1, memoryProperties));
}

TEST_F(InitializedHostUnifiedMemoryPoolingTest, givenVariousPointersWhenCallingIsInPoolThenCorrectValueIsReturned) {
    void *ptrBeforePool = reinterpret_cast<void *>(reinterpret_cast<size_t>(usmMemAllocPool.pool) - 1);
    void *lastPtrInPool = reinterpret_cast<void *>(reinterpret_cast<size_t>(usmMemAllocPool.poolEnd) - 1);

    EXPECT_FALSE(usmMemAllocPool.isInPool(ptrBeforePool));
    EXPECT_TRUE(usmMemAllocPool.isInPool(usmMemAllocPool.pool));
    EXPECT_TRUE(usmMemAllocPool.isInPool(lastPtrInPool));
    EXPECT_FALSE(usmMemAllocPool.isInPool(usmMemAllocPool.poolEnd));
}

TEST_F(InitializedHostUnifiedMemoryPoolingTest, givenPoolableAllocationWhenUsingPoolThenAllocationIsPooledUnlessPoolIsFull) {
    SVMAllocsManager::UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize64k, rootDeviceIndices, deviceBitfields);
    const auto allocationSize = UsmMemAllocPool::allocationThreshold;
    const auto allocationSizeAboveThreshold = allocationSize + 1;
    EXPECT_EQ(nullptr, usmMemAllocPool.createUnifiedMemoryAllocation(allocationSizeAboveThreshold, memoryProperties));
    EXPECT_EQ(nullptr, usmMemAllocPool.allocations.get(reinterpret_cast<void *>(0x1)));

    auto allocFromPool = usmMemAllocPool.createUnifiedMemoryAllocation(allocationSize, memoryProperties);
    EXPECT_NE(nullptr, allocFromPool);
    EXPECT_TRUE(usmMemAllocPool.isInPool(allocFromPool));
    auto allocationInfo = usmMemAllocPool.allocations.get(allocFromPool);
    EXPECT_NE(nullptr, allocationInfo);
    EXPECT_EQ(allocationSize, allocationInfo->size);

    auto svmData = svmManager->getSVMAlloc(allocFromPool);
    auto poolSvmData = svmManager->getSVMAlloc(usmMemAllocPool.pool);
    EXPECT_EQ(svmData, poolSvmData);

    const auto allocationsToFillPool = poolSize / allocationSize;
    for (auto i = 1u; i < allocationsToFillPool; ++i) {
        // exhaust pool
        EXPECT_NE(nullptr, usmMemAllocPool.createUnifiedMemoryAllocation(allocationSize, memoryProperties));
    }

    EXPECT_EQ(nullptr, usmMemAllocPool.createUnifiedMemoryAllocation(1, memoryProperties));

    EXPECT_FALSE(usmMemAllocPool.freeSVMAlloc(reinterpret_cast<void *>(0x1), true));
    EXPECT_TRUE(usmMemAllocPool.freeSVMAlloc(allocFromPool, true));
    EXPECT_FALSE(usmMemAllocPool.freeSVMAlloc(allocFromPool, true));
    EXPECT_EQ(nullptr, usmMemAllocPool.allocations.get(reinterpret_cast<void *>(0x1)));
    EXPECT_EQ(nullptr, usmMemAllocPool.allocations.extract(reinterpret_cast<void *>(0x1)));

    EXPECT_NE(nullptr, usmMemAllocPool.createUnifiedMemoryAllocation(allocationSize, memoryProperties));
}

using InitializationFailedUnifiedMemoryPoolingTest = InitializedUnifiedMemoryPoolingTest<InternalMemoryType::hostUnifiedMemory, true>;
TEST_F(InitializationFailedUnifiedMemoryPoolingTest, givenNotInitializedPoolWhenUsingPoolThenMethodsSucceed) {
    SVMAllocsManager::UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize64k, rootDeviceIndices, deviceBitfields);
    const auto allocationSize = UsmMemAllocPool::allocationThreshold;
    EXPECT_EQ(nullptr, usmMemAllocPool.createUnifiedMemoryAllocation(allocationSize, memoryProperties));
    EXPECT_FALSE(usmMemAllocPool.freeSVMAlloc(reinterpret_cast<void *>(0x1), true));
}
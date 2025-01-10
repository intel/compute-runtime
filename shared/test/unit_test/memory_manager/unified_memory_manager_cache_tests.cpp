/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/api_specific_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/mocks/mock_ail_configuration.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"
namespace NEO {

extern ApiSpecificConfig::ApiType apiTypeForUlts;

TEST(SortedVectorBasedAllocationTrackerTests, givenSortedVectorBasedAllocationTrackerWhenInsertRemoveAndGetThenStoreDataProperly) {
    SvmAllocationData data(1u);
    SVMAllocsManager::SortedVectorBasedAllocationTracker tracker;

    MockGraphicsAllocation graphicsAllocations[] = {{reinterpret_cast<void *>(0x1 * MemoryConstants::pageSize64k), MemoryConstants::pageSize64k},
                                                    {reinterpret_cast<void *>(0x2 * MemoryConstants::pageSize64k), MemoryConstants::pageSize64k},
                                                    {reinterpret_cast<void *>(0x3 * MemoryConstants::pageSize64k), MemoryConstants::pageSize64k},
                                                    {reinterpret_cast<void *>(0x4 * MemoryConstants::pageSize64k), MemoryConstants::pageSize64k},
                                                    {reinterpret_cast<void *>(0x5 * MemoryConstants::pageSize64k), MemoryConstants::pageSize64k},
                                                    {reinterpret_cast<void *>(0x6 * MemoryConstants::pageSize64k), MemoryConstants::pageSize64k},
                                                    {reinterpret_cast<void *>(0x7 * MemoryConstants::pageSize64k), MemoryConstants::pageSize64k},
                                                    {reinterpret_cast<void *>(0x8 * MemoryConstants::pageSize64k), MemoryConstants::pageSize64k},
                                                    {reinterpret_cast<void *>(0x9 * MemoryConstants::pageSize64k), MemoryConstants::pageSize64k},
                                                    {reinterpret_cast<void *>(0xA * MemoryConstants::pageSize64k), MemoryConstants::pageSize64k}};
    const auto graphicsAllocationsSize = sizeof(graphicsAllocations) / sizeof(MockGraphicsAllocation);
    for (uint32_t i = graphicsAllocationsSize - 1; i >= graphicsAllocationsSize / 2; --i) {
        data.gpuAllocations.addAllocation(&graphicsAllocations[i]);
        data.device = reinterpret_cast<Device *>(graphicsAllocations[i].getGpuAddress());
        tracker.insert(reinterpret_cast<void *>(data.gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()), data);
    }
    for (uint32_t i = 0; i < graphicsAllocationsSize / 2; ++i) {
        data.gpuAllocations.addAllocation(&graphicsAllocations[i]);
        data.device = reinterpret_cast<Device *>(graphicsAllocations[i].getGpuAddress());
        tracker.insert(reinterpret_cast<void *>(data.gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()), data);
    }

    EXPECT_EQ(tracker.getNumAllocs(), graphicsAllocationsSize);
    for (uint64_t i = 0; i < graphicsAllocationsSize; ++i) {
        EXPECT_EQ((i + 1) * MemoryConstants::pageSize64k, reinterpret_cast<uint64_t>(tracker.allocations[static_cast<uint32_t>(i)].first));
        EXPECT_EQ((i + 1) * MemoryConstants::pageSize64k, reinterpret_cast<uint64_t>(tracker.allocations[static_cast<uint32_t>(i)].second->device));
    }

    auto addr1 = reinterpret_cast<void *>(graphicsAllocations[7].getGpuAddress());
    auto data1 = tracker.get(addr1);
    EXPECT_EQ(data1->device, addr1);

    MockGraphicsAllocation graphicsAlloc{reinterpret_cast<void *>(0x0), MemoryConstants::pageSize64k};
    data.gpuAllocations.addAllocation(&graphicsAlloc);
    data.device = reinterpret_cast<Device *>(graphicsAlloc.getGpuAddress());
    tracker.insert(reinterpret_cast<void *>(data.gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()), data);

    EXPECT_EQ(tracker.getNumAllocs(), graphicsAllocationsSize + 1);
    for (uint64_t i = 0; i < graphicsAllocationsSize + 1; ++i) {
        EXPECT_EQ(i * MemoryConstants::pageSize64k, reinterpret_cast<uint64_t>(tracker.allocations[static_cast<uint32_t>(i)].first));
        EXPECT_EQ(i * MemoryConstants::pageSize64k, reinterpret_cast<uint64_t>(tracker.allocations[static_cast<uint32_t>(i)].second->device));
    }
    EXPECT_EQ(data1->device, addr1);

    auto addr2 = reinterpret_cast<void *>(graphicsAllocations[1].getGpuAddress());
    auto data2 = tracker.get(addr2);
    EXPECT_EQ(data1->device, addr1);
    EXPECT_EQ(data2->device, addr2);
    tracker.remove(addr2);
    EXPECT_EQ(tracker.getNumAllocs(), graphicsAllocationsSize);
    for (uint64_t i = 0; i < graphicsAllocationsSize; ++i) {
        if (i < 2) {
            EXPECT_EQ(i * MemoryConstants::pageSize64k, reinterpret_cast<uint64_t>(tracker.allocations[static_cast<uint32_t>(i)].first));
            EXPECT_EQ(i * MemoryConstants::pageSize64k, reinterpret_cast<uint64_t>(tracker.allocations[static_cast<uint32_t>(i)].second->device));
        } else {
            EXPECT_EQ((i + 1) * MemoryConstants::pageSize64k, reinterpret_cast<uint64_t>(tracker.allocations[static_cast<uint32_t>(i)].first));
            EXPECT_EQ((i + 1) * MemoryConstants::pageSize64k, reinterpret_cast<uint64_t>(tracker.allocations[static_cast<uint32_t>(i)].second->device));
        }
    }
    EXPECT_EQ(data1->device, addr1);
}

using SvmAllocationCacheSimpleTest = ::testing::Test;

TEST(SvmAllocationCacheSimpleTest, givenDifferentSizesWhenCheckingIfAllocUtilizationAllowedThenReturnCorrectValue) {
    static constexpr size_t allocationSizeBasis = SVMAllocsManager::SvmAllocationCache::minimalSizeToCheckUtilization;
    EXPECT_TRUE(SVMAllocsManager::SvmAllocationCache::allocUtilizationAllows(1u, allocationSizeBasis - 1));
    EXPECT_TRUE(SVMAllocsManager::SvmAllocationCache::allocUtilizationAllows(allocationSizeBasis - 1, allocationSizeBasis - 1));
    EXPECT_FALSE(SVMAllocsManager::SvmAllocationCache::allocUtilizationAllows(allocationSizeBasis / 2 - 1, allocationSizeBasis));
    EXPECT_TRUE(SVMAllocsManager::SvmAllocationCache::allocUtilizationAllows(allocationSizeBasis / 2, allocationSizeBasis));
    EXPECT_TRUE(SVMAllocsManager::SvmAllocationCache::allocUtilizationAllows(allocationSizeBasis, allocationSizeBasis));
}

TEST(SvmAllocationCacheSimpleTest, givenDifferentSizesWhenCheckingIfSizeAllowsThenReturnCorrectValue) {
    EXPECT_TRUE(SVMAllocsManager::SvmAllocationCache::sizeAllowed(256 * MemoryConstants::megaByte));
    EXPECT_FALSE(SVMAllocsManager::SvmAllocationCache::sizeAllowed(256 * MemoryConstants::megaByte + 1));
}

TEST(SvmAllocationCacheSimpleTest, givenAllocationsWhenCheckingIsInUseThenReturnCorrectValue) {
    SVMAllocsManager::SvmAllocationCache allocationCache;
    MockMemoryManager memoryManager;
    MockSVMAllocsManager svmAllocsManager(&memoryManager, false);

    allocationCache.memoryManager = &memoryManager;
    allocationCache.svmAllocsManager = &svmAllocsManager;

    {
        memoryManager.deferAllocInUse = false;
        MockGraphicsAllocation gpuGfxAllocation;
        SvmAllocationData svmAllocData(mockRootDeviceIndex);
        EXPECT_FALSE(allocationCache.isInUse(&svmAllocData));
        svmAllocData.gpuAllocations.addAllocation(&gpuGfxAllocation);
        EXPECT_FALSE(allocationCache.isInUse(&svmAllocData));
        memoryManager.deferAllocInUse = true;
        EXPECT_TRUE(allocationCache.isInUse(&svmAllocData));
    }
    {
        memoryManager.deferAllocInUse = false;
        MockGraphicsAllocation cpuGfxAllocation;
        SvmAllocationData svmAllocData(mockRootDeviceIndex);
        svmAllocData.cpuAllocation = &cpuGfxAllocation;
        EXPECT_FALSE(allocationCache.isInUse(&svmAllocData));
        memoryManager.deferAllocInUse = true;
        EXPECT_TRUE(allocationCache.isInUse(&svmAllocData));
    }
}

struct SvmAllocationCacheTestFixture {
    SvmAllocationCacheTestFixture() : executionEnvironment(defaultHwInfo.get()) {}
    void setUp() {
        bool svmSupported = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->capabilityTable.ftrSvm;
        if (!svmSupported) {
            GTEST_SKIP();
        }
    }
    void tearDown() {
    }
    static constexpr size_t allocationSizeBasis = MemoryConstants::pageSize64k;
    MockExecutionEnvironment executionEnvironment;
};

using SvmDeviceAllocationCacheTest = Test<SvmAllocationCacheTestFixture>;

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationCacheDisabledWhenCheckingIfEnabledThenItIsDisabled) {
    DebugManagerStateRestore restorer;
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(0);
    EXPECT_FALSE(svmManager->usmDeviceAllocationsCacheEnabled);
    svmManager->initUsmAllocationsCaches(*device);
    EXPECT_FALSE(svmManager->usmDeviceAllocationsCacheEnabled);
}

HWTEST_F(SvmDeviceAllocationCacheTest, givenOclApiSpecificConfigWhenCheckingIfEnabledItIsEnabledIfProductHelperMethodReturnsTrue) {
    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    auto device = deviceFactory->rootDevices[0];
    RAIIProductHelperFactory<MockProductHelper> raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    MockAILConfiguration mockAilConfigurationHelper;
    device->mockAilConfigurationHelper = &mockAilConfigurationHelper;
    {
        raii.mockProductHelper->isDeviceUsmAllocationReuseSupportedResult = false;
        auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
        EXPECT_FALSE(svmManager->usmDeviceAllocationsCacheEnabled);
        svmManager->initUsmAllocationsCaches(*device);
        EXPECT_FALSE(svmManager->usmDeviceAllocationsCacheEnabled);
    }
    {
        raii.mockProductHelper->isDeviceUsmAllocationReuseSupportedResult = true;
        auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
        EXPECT_FALSE(svmManager->usmDeviceAllocationsCacheEnabled);
        svmManager->initUsmAllocationsCaches(*device);
        EXPECT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);
        const auto expectedMaxSize = static_cast<size_t>(0.08 * device->getGlobalMemorySize(static_cast<uint32_t>(device->getDeviceBitfield().to_ullong())));
        EXPECT_EQ(expectedMaxSize, svmManager->usmDeviceAllocationsCache.maxSize);
    }
    {
        raii.mockProductHelper->isDeviceUsmAllocationReuseSupportedResult = true;
        mockAilConfigurationHelper.limitAmountOfDeviceMemoryForRecyclingReturn = true;
        auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
        EXPECT_FALSE(svmManager->usmDeviceAllocationsCacheEnabled);
        svmManager->initUsmAllocationsCaches(*device);
        EXPECT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);
        const auto expectedMaxSize = 0u;
        EXPECT_EQ(expectedMaxSize, svmManager->usmDeviceAllocationsCache.maxSize);
    }
}

struct SvmDeviceAllocationCacheSimpleTestDataType {
    size_t allocationSize;
    void *allocation;
};

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationCacheEnabledWhenFreeingDeviceAllocationThenItIsPutIntoCache) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);
    svmManager->usmDeviceAllocationsCache.maxSize = 1 * MemoryConstants::gigaByte;

    auto testDataset = std::vector<SvmDeviceAllocationCacheSimpleTestDataType>(
        {{1u, nullptr},
         {(allocationSizeBasis * 1) - 1, nullptr},
         {(allocationSizeBasis * 1), nullptr},
         {(allocationSizeBasis * 1) + 1, nullptr},
         {(allocationSizeBasis * 2) - 1, nullptr},
         {(allocationSizeBasis * 2), nullptr},
         {(allocationSizeBasis * 2) + 1, nullptr}});

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    for (auto &testData : testDataset) {
        testData.allocation = svmManager->createUnifiedMemoryAllocation(testData.allocationSize, unifiedMemoryProperties);
        ASSERT_NE(testData.allocation, nullptr);
    }
    size_t expectedCacheSize = 0u;
    ASSERT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), expectedCacheSize);

    for (auto const &testData : testDataset) {
        svmManager->freeSVMAlloc(testData.allocation);
        EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), ++expectedCacheSize);
        bool foundInCache = false;
        for (auto i = 0u; i < svmManager->usmDeviceAllocationsCache.allocations.size(); ++i) {
            if (svmManager->usmDeviceAllocationsCache.allocations[i].allocation == testData.allocation) {
                foundInCache = true;
                break;
            }
        }
        EXPECT_TRUE(foundInCache);
    }
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), testDataset.size());

    svmManager->trimUSMDeviceAllocCache();
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 0u);
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationCacheEnabledWhenInitializedThenMaxSizeIsSetCorrectly) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    auto device = deviceFactory->rootDevices[0];
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(2);
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);

    auto expectedMaxSize = static_cast<size_t>(device->getGlobalMemorySize(static_cast<uint32_t>(mockDeviceBitfield.to_ulong())) * 0.02);
    EXPECT_EQ(expectedMaxSize, svmManager->usmDeviceAllocationsCache.maxSize);
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationCacheEnabledWhenFreeingDeviceAllocationThenItIsPutIntoCacheOnlyIfMaxSizeWillNotBeExceeded) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);

    constexpr auto allocationSize = MemoryConstants::pageSize64k;
    svmManager->usmDeviceAllocationsCache.maxSize = allocationSize;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    {
        auto allocation = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation, nullptr);
        auto allocation2 = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation2, nullptr);
        EXPECT_EQ(0u, svmManager->usmDeviceAllocationsCache.allocations.size());
        EXPECT_EQ(0u, device->getAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(allocation);
        EXPECT_EQ(1u, svmManager->usmDeviceAllocationsCache.allocations.size());
        EXPECT_EQ(allocationSize, device->getAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(allocation2);
        EXPECT_EQ(1u, svmManager->usmDeviceAllocationsCache.allocations.size());
        EXPECT_EQ(allocationSize, device->getAllocationsSavedForReuseSize());

        auto recycledAllocation = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(recycledAllocation, allocation);
        EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 0u);
        EXPECT_EQ(0u, device->getAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(recycledAllocation);

        svmManager->trimUSMDeviceAllocCache();
        EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 0u);
        EXPECT_EQ(0u, device->getAllocationsSavedForReuseSize());
    }
    {
        auto allocation = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation, nullptr);
        auto allocation2 = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation2, nullptr);
        EXPECT_EQ(0u, svmManager->usmDeviceAllocationsCache.allocations.size());
        EXPECT_EQ(0u, device->getAllocationsSavedForReuseSize());

        svmManager->freeSVMAllocDefer(allocation);
        EXPECT_EQ(1u, svmManager->usmDeviceAllocationsCache.allocations.size());
        EXPECT_EQ(allocationSize, device->getAllocationsSavedForReuseSize());

        svmManager->freeSVMAllocDefer(allocation2);
        EXPECT_EQ(1u, svmManager->usmDeviceAllocationsCache.allocations.size());
        EXPECT_EQ(allocationSize, device->getAllocationsSavedForReuseSize());

        auto recycledAllocation = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(recycledAllocation, allocation);
        EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 0u);
        EXPECT_EQ(0u, device->getAllocationsSavedForReuseSize());

        svmManager->freeSVMAllocDefer(recycledAllocation);

        svmManager->trimUSMDeviceAllocCache();
        EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 0u);
        EXPECT_EQ(0u, device->getAllocationsSavedForReuseSize());
    }
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationCacheEnabledAndMultipleSVMManagersWhenFreeingDeviceAllocationThenItIsPutIntoCacheOnlyIfMaxSizeWillNotBeExceeded) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    auto secondSvmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*device);
    secondSvmManager->initUsmAllocationsCaches(*device);
    ASSERT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);
    ASSERT_TRUE(secondSvmManager->usmDeviceAllocationsCacheEnabled);

    constexpr auto allocationSize = MemoryConstants::pageSize64k;
    svmManager->usmDeviceAllocationsCache.maxSize = allocationSize;
    secondSvmManager->usmDeviceAllocationsCache.maxSize = allocationSize;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    {
        auto allocation = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation, nullptr);
        auto allocation2 = secondSvmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation2, nullptr);

        svmManager->freeSVMAlloc(allocation);
        EXPECT_EQ(1u, svmManager->usmDeviceAllocationsCache.allocations.size());
        EXPECT_EQ(allocationSize, device->getAllocationsSavedForReuseSize());

        secondSvmManager->freeSVMAlloc(allocation2);
        EXPECT_EQ(0u, secondSvmManager->usmDeviceAllocationsCache.allocations.size());
        EXPECT_EQ(allocationSize, device->getAllocationsSavedForReuseSize());

        auto recycledAllocation = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(recycledAllocation, allocation);
        EXPECT_EQ(0u, svmManager->usmDeviceAllocationsCache.allocations.size());
        EXPECT_EQ(0u, device->getAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(recycledAllocation);

        svmManager->trimUSMDeviceAllocCache();
        EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 0u);
        EXPECT_EQ(0u, device->getAllocationsSavedForReuseSize());
    }
    {
        auto allocation = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation, nullptr);
        auto allocation2 = secondSvmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation2, nullptr);

        secondSvmManager->freeSVMAlloc(allocation2);
        EXPECT_EQ(1u, secondSvmManager->usmDeviceAllocationsCache.allocations.size());
        EXPECT_EQ(allocationSize, device->getAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(allocation);
        EXPECT_EQ(0u, svmManager->usmDeviceAllocationsCache.allocations.size());
        EXPECT_EQ(allocationSize, device->getAllocationsSavedForReuseSize());

        auto recycledAllocation = secondSvmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(recycledAllocation, allocation2);
        EXPECT_EQ(0u, secondSvmManager->usmDeviceAllocationsCache.allocations.size());
        EXPECT_EQ(0u, device->getAllocationsSavedForReuseSize());

        secondSvmManager->freeSVMAlloc(recycledAllocation);

        secondSvmManager->trimUSMDeviceAllocCache();
        EXPECT_EQ(secondSvmManager->usmDeviceAllocationsCache.allocations.size(), 0u);
        EXPECT_EQ(0u, device->getAllocationsSavedForReuseSize());
    }
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationsWithDifferentSizesWhenAllocatingAfterFreeThenReturnCorrectCachedAllocation) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);
    svmManager->usmDeviceAllocationsCache.maxSize = 1 * MemoryConstants::gigaByte;

    auto testDataset = std::vector<SvmDeviceAllocationCacheSimpleTestDataType>(
        {
            {(allocationSizeBasis * 1), nullptr},
            {(allocationSizeBasis * 2), nullptr},
            {(allocationSizeBasis * 3), nullptr},
        });

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    for (auto &testData : testDataset) {
        testData.allocation = svmManager->createUnifiedMemoryAllocation(testData.allocationSize, unifiedMemoryProperties);
        ASSERT_NE(testData.allocation, nullptr);
    }

    size_t expectedCacheSize = 0u;
    ASSERT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), expectedCacheSize);

    for (auto const &testData : testDataset) {
        svmManager->freeSVMAlloc(testData.allocation);
    }

    ASSERT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), testDataset.size());

    std::vector<void *> allocationsToFree;

    for (auto &testData : testDataset) {
        auto secondAllocation = svmManager->createUnifiedMemoryAllocation(testData.allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), testDataset.size() - 1);
        EXPECT_EQ(secondAllocation, testData.allocation);
        svmManager->freeSVMAlloc(secondAllocation);
        EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), testDataset.size());
    }

    svmManager->trimUSMDeviceAllocCache();
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 0u);
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationsWithDifferentSizesWhenAllocatingAfterFreeThenLimitMemoryWastage) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);
    svmManager->usmDeviceAllocationsCache.maxSize = 1 * MemoryConstants::gigaByte;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    auto allocation = svmManager->createUnifiedMemoryAllocation(SVMAllocsManager::SvmAllocationCache::minimalSizeToCheckUtilization, unifiedMemoryProperties);
    ASSERT_NE(allocation, nullptr);

    svmManager->freeSVMAlloc(allocation);

    ASSERT_EQ(1u, svmManager->usmDeviceAllocationsCache.allocations.size());

    constexpr auto allowedSizeForReuse = static_cast<size_t>(SVMAllocsManager::SvmAllocationCache::minimalSizeToCheckUtilization * SVMAllocsManager::SvmAllocationCache::minimalAllocUtilization);
    constexpr auto notAllowedSizeDueToMemoryWastage = allowedSizeForReuse - 1u;

    auto notReusedDueToMemoryWastage = svmManager->createUnifiedMemoryAllocation(notAllowedSizeDueToMemoryWastage, unifiedMemoryProperties);
    EXPECT_NE(nullptr, notReusedDueToMemoryWastage);
    EXPECT_NE(notReusedDueToMemoryWastage, allocation);
    EXPECT_EQ(1u, svmManager->usmDeviceAllocationsCache.allocations.size());

    auto reused = svmManager->createUnifiedMemoryAllocation(allowedSizeForReuse, unifiedMemoryProperties);
    EXPECT_NE(nullptr, notReusedDueToMemoryWastage);
    EXPECT_EQ(reused, allocation);
    EXPECT_EQ(0u, svmManager->usmDeviceAllocationsCache.allocations.size());

    svmManager->freeSVMAlloc(notReusedDueToMemoryWastage);
    svmManager->freeSVMAlloc(reused);
    svmManager->trimUSMDeviceAllocCache();
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 0u);
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationOverSizeLimitWhenAllocatingAfterFreeThenDontSaveForReuse) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);
    svmManager->usmDeviceAllocationsCache.maxSize = 1 * MemoryConstants::gigaByte;
    const auto notAcceptedAllocSize = SVMAllocsManager::SvmAllocationCache::maxServicedSize + 1;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    auto mockMemoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    auto mockGa = std::make_unique<MockGraphicsAllocation>(mockRootDeviceIndex, nullptr, notAcceptedAllocSize);
    mockGa->gpuAddress = 0xbadf00;
    mockGa->cpuPtr = reinterpret_cast<void *>(0xbadf00);
    mockGa->setAllocationType(AllocationType::svmGpu);
    mockMemoryManager->mockGa = mockGa.release();
    mockMemoryManager->returnMockGAFromDevicePool = true;
    auto allocation = svmManager->createUnifiedMemoryAllocation(notAcceptedAllocSize, unifiedMemoryProperties);
    EXPECT_EQ(reinterpret_cast<void *>(0xbadf00), allocation);

    svmManager->freeSVMAlloc(allocation);

    EXPECT_EQ(0u, svmManager->usmDeviceAllocationsCache.allocations.size());
}

TEST_F(SvmDeviceAllocationCacheTest, givenMultipleAllocationsWhenAllocatingAfterFreeThenReturnAllocationsInCacheStartingFromSmallest) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);
    svmManager->usmDeviceAllocationsCache.maxSize = 1 * MemoryConstants::gigaByte;

    auto testDataset = std::vector<SvmDeviceAllocationCacheSimpleTestDataType>(
        {
            {(allocationSizeBasis * 1), nullptr},
            {(allocationSizeBasis * 2), nullptr},
            {(allocationSizeBasis * 3), nullptr},
        });

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    for (auto &testData : testDataset) {
        testData.allocation = svmManager->createUnifiedMemoryAllocation(testData.allocationSize, unifiedMemoryProperties);
        ASSERT_NE(testData.allocation, nullptr);
    }

    ASSERT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 0u);

    for (auto const &testData : testDataset) {
        svmManager->freeSVMAlloc(testData.allocation);
    }

    size_t expectedCacheSize = testDataset.size();
    ASSERT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), expectedCacheSize);

    auto allocationLargerThanInCache = svmManager->createUnifiedMemoryAllocation(allocationSizeBasis << 3, unifiedMemoryProperties);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), expectedCacheSize);

    auto firstAllocation = svmManager->createUnifiedMemoryAllocation(allocationSizeBasis, unifiedMemoryProperties);
    EXPECT_EQ(firstAllocation, testDataset[0].allocation);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), --expectedCacheSize);

    auto secondAllocation = svmManager->createUnifiedMemoryAllocation(allocationSizeBasis, unifiedMemoryProperties);
    EXPECT_EQ(secondAllocation, testDataset[1].allocation);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), --expectedCacheSize);

    auto thirdAllocation = svmManager->createUnifiedMemoryAllocation(allocationSizeBasis, unifiedMemoryProperties);
    EXPECT_EQ(thirdAllocation, testDataset[2].allocation);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 0u);

    svmManager->freeSVMAlloc(firstAllocation);
    svmManager->freeSVMAlloc(secondAllocation);
    svmManager->freeSVMAlloc(thirdAllocation);
    svmManager->freeSVMAlloc(allocationLargerThanInCache);

    svmManager->trimUSMDeviceAllocCache();
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 0u);
}

struct SvmDeviceAllocationCacheTestDataType {
    SvmDeviceAllocationCacheTestDataType(size_t allocationSize,
                                         const RootDeviceIndicesContainer &rootDeviceIndicesArg,
                                         std::map<uint32_t, DeviceBitfield> &subdeviceBitFields,
                                         Device *device,
                                         std::string name) : allocationSize(allocationSize),
                                                             unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory,
                                                                                     1,
                                                                                     rootDeviceIndicesArg,
                                                                                     subdeviceBitFields),
                                                             name(name) {
        unifiedMemoryProperties.device = device;
    };
    size_t allocationSize;
    void *allocation{nullptr};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties;
    std::string name;
};

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationsWithDifferentFlagsWhenAllocatingAfterFreeThenReturnCorrectAllocation) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(2, 2));
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto rootDevice = deviceFactory->rootDevices[0];
    auto secondRootDevice = deviceFactory->rootDevices[1];
    auto subDevice1 = deviceFactory->subDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(rootDevice->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*rootDevice);
    ASSERT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);
    svmManager->usmDeviceAllocationsCache.maxSize = 1 * MemoryConstants::gigaByte;

    size_t defaultAllocSize = allocationSizeBasis;
    std::map<uint32_t, DeviceBitfield> subDeviceBitfields = {{0u, {01}}, {1u, {10}}};
    SvmDeviceAllocationCacheTestDataType
        defaultAlloc(defaultAllocSize,
                     {rootDevice->getRootDeviceIndex()},
                     subDeviceBitfields,
                     rootDevice, "defaultAlloc"),
        writeOnly(defaultAllocSize,
                  {rootDevice->getRootDeviceIndex()},
                  subDeviceBitfields,
                  rootDevice, "writeOnly"),
        readOnly(defaultAllocSize,
                 {rootDevice->getRootDeviceIndex()},
                 subDeviceBitfields,
                 rootDevice, "readOnly"),
        allocWriteCombined(defaultAllocSize,
                           {rootDevice->getRootDeviceIndex()},
                           subDeviceBitfields,
                           rootDevice, "allocWriteCombined"),
        secondDevice(defaultAllocSize,
                     {rootDevice->getRootDeviceIndex()},
                     subDeviceBitfields,
                     secondRootDevice, "secondDevice"),
        subDevice(defaultAllocSize,
                  {rootDevice->getRootDeviceIndex()},
                  subDeviceBitfields,
                  subDevice1, "subDevice");
    writeOnly.unifiedMemoryProperties.allocationFlags.flags.writeOnly = true;
    readOnly.unifiedMemoryProperties.allocationFlags.flags.readOnly = true;
    allocWriteCombined.unifiedMemoryProperties.allocationFlags.allocFlags.allocWriteCombined = true;

    auto testDataset = std::vector<SvmDeviceAllocationCacheTestDataType>({defaultAlloc, writeOnly, readOnly, allocWriteCombined, secondDevice, subDevice});
    for (auto &allocationDataToVerify : testDataset) {

        for (auto &testData : testDataset) {
            testData.allocation = svmManager->createUnifiedMemoryAllocation(testData.allocationSize, testData.unifiedMemoryProperties);
        }
        ASSERT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 0u);

        for (auto &testData : testDataset) {
            svmManager->freeSVMAlloc(testData.allocation);
        }
        ASSERT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), testDataset.size());

        auto allocationFromCache = svmManager->createUnifiedMemoryAllocation(allocationDataToVerify.allocationSize, allocationDataToVerify.unifiedMemoryProperties);
        EXPECT_EQ(allocationFromCache, allocationDataToVerify.allocation);

        auto allocationNotFromCache = svmManager->createUnifiedMemoryAllocation(allocationDataToVerify.allocationSize, allocationDataToVerify.unifiedMemoryProperties);
        for (auto &cachedAllocation : testDataset) {
            EXPECT_NE(allocationNotFromCache, cachedAllocation.allocation);
        }
        svmManager->freeSVMAlloc(allocationFromCache);
        svmManager->freeSVMAlloc(allocationNotFromCache);

        svmManager->trimUSMDeviceAllocCache();
        ASSERT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 0u);
    }
}

TEST_F(SvmDeviceAllocationCacheTest, givenDeviceOutOfMemoryWhenAllocatingThenCacheIsTrimmedAndAllocationSucceeds) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    device->injectMemoryManager(new MockMemoryManagerWithCapacity(*device->getExecutionEnvironment()));
    MockMemoryManagerWithCapacity *memoryManager = static_cast<MockMemoryManagerWithCapacity *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager, false);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);
    svmManager->usmDeviceAllocationsCache.maxSize = 1 * MemoryConstants::gigaByte;

    memoryManager->capacity = MemoryConstants::pageSize64k * 3;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;

    auto allocationInCache = svmManager->createUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties);
    auto allocationInCache2 = svmManager->createUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties);
    auto allocationInCache3 = svmManager->createUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties);
    ASSERT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 0u);
    svmManager->freeSVMAlloc(allocationInCache);
    svmManager->freeSVMAlloc(allocationInCache2);
    svmManager->freeSVMAllocDefer(allocationInCache3);

    ASSERT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 3u);
    ASSERT_NE(svmManager->getSVMAlloc(allocationInCache), nullptr);
    ASSERT_NE(svmManager->getSVMAlloc(allocationInCache2), nullptr);
    ASSERT_NE(svmManager->getSVMAlloc(allocationInCache3), nullptr);
    auto ptr = svmManager->createUnifiedMemoryAllocation(MemoryConstants::pageSize64k * 2, unifiedMemoryProperties);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 0u);
    svmManager->freeSVMAlloc(ptr);

    svmManager->trimUSMDeviceAllocCache();
    ASSERT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 0u);
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationWithIsInternalAllocationSetWhenAllocatingAfterFreeThenDoNotReuseAllocation) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*device);
    EXPECT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);
    svmManager->usmDeviceAllocationsCache.maxSize = 1 * MemoryConstants::gigaByte;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    auto allocation = svmManager->createUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    EXPECT_NE(allocation, nullptr);
    svmManager->freeSVMAlloc(allocation);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 1u);

    unifiedMemoryProperties.isInternalAllocation = true;
    auto testedAllocation = svmManager->createUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 1u);
    auto svmData = svmManager->getSVMAlloc(testedAllocation);
    EXPECT_NE(nullptr, svmData);
    EXPECT_TRUE(svmData->isInternalAllocation);

    svmManager->freeSVMAlloc(testedAllocation);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 1u);

    svmManager->trimUSMDeviceAllocCache();
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationInUsageWhenAllocatingAfterFreeThenDoNotReuseAllocation) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*device);
    EXPECT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);
    svmManager->usmDeviceAllocationsCache.maxSize = 1 * MemoryConstants::gigaByte;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    auto allocation = svmManager->createUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    EXPECT_NE(allocation, nullptr);
    svmManager->freeSVMAlloc(allocation);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 1u);

    MockMemoryManager *mockMemoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    mockMemoryManager->deferAllocInUse = true;
    auto testedAllocation = svmManager->createUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 1u);
    auto svmData = svmManager->getSVMAlloc(testedAllocation);
    EXPECT_NE(nullptr, svmData);

    svmManager->freeSVMAlloc(testedAllocation);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.allocations.size(), 2u);

    svmManager->trimUSMDeviceAllocCache();
}

using SvmHostAllocationCacheTest = Test<SvmAllocationCacheTestFixture>;

TEST_F(SvmHostAllocationCacheTest, givenAllocationCacheDisabledWhenCheckingIfEnabledThenItIsDisabled) {
    DebugManagerStateRestore restorer;
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(0);
    EXPECT_FALSE(svmManager->usmHostAllocationsCacheEnabled);
    svmManager->initUsmAllocationsCaches(*device);
    EXPECT_FALSE(svmManager->usmHostAllocationsCacheEnabled);

    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto allocation = svmManager->createHostUnifiedMemoryAllocation(1u, unifiedMemoryProperties);
    EXPECT_NE(allocation, nullptr);
    svmManager->freeSVMAlloc(allocation);
    EXPECT_EQ(0u, svmManager->usmHostAllocationsCache.allocations.size());

    allocation = svmManager->createHostUnifiedMemoryAllocation(1u, unifiedMemoryProperties);
    EXPECT_NE(allocation, nullptr);
    svmManager->freeSVMAllocDefer(allocation);
    EXPECT_EQ(0u, svmManager->usmHostAllocationsCache.allocations.size());
}

HWTEST_F(SvmHostAllocationCacheTest, givenOclApiSpecificConfigWhenCheckingIfEnabledItIsEnabledIfProductHelperMethodReturnsTrue) {
    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    auto device = deviceFactory->rootDevices[0];
    RAIIProductHelperFactory<MockProductHelper> raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    {
        raii.mockProductHelper->isHostUsmAllocationReuseSupportedResult = false;
        auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
        EXPECT_FALSE(svmManager->usmHostAllocationsCacheEnabled);
        svmManager->initUsmAllocationsCaches(*device);
        EXPECT_FALSE(svmManager->usmHostAllocationsCacheEnabled);
    }
    {
        raii.mockProductHelper->isHostUsmAllocationReuseSupportedResult = true;
        auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
        EXPECT_FALSE(svmManager->usmHostAllocationsCacheEnabled);
        svmManager->initUsmAllocationsCaches(*device);
        EXPECT_TRUE(svmManager->usmHostAllocationsCacheEnabled);
        const auto expectedMaxSize = static_cast<size_t>(0.02 * svmManager->memoryManager->getSystemSharedMemory(0u));
        EXPECT_EQ(expectedMaxSize, svmManager->usmHostAllocationsCache.maxSize);
    }
}

struct SvmHostAllocationCacheSimpleTestDataType {
    size_t allocationSize;
    void *allocation;
};

TEST_F(SvmHostAllocationCacheTest, givenAllocationCacheEnabledWhenFreeingHostAllocationThenItIsPutIntoCache) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_TRUE(svmManager->usmHostAllocationsCacheEnabled);
    svmManager->usmHostAllocationsCache.maxSize = 1 * MemoryConstants::gigaByte;

    auto testDataset = std::vector<SvmHostAllocationCacheSimpleTestDataType>(
        {{1u, nullptr},
         {(allocationSizeBasis * 1) - 1, nullptr},
         {(allocationSizeBasis * 1), nullptr},
         {(allocationSizeBasis * 1) + 1, nullptr},
         {(allocationSizeBasis * 2) - 1, nullptr},
         {(allocationSizeBasis * 2), nullptr},
         {(allocationSizeBasis * 2) + 1, nullptr}});

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    for (auto &testData : testDataset) {
        testData.allocation = svmManager->createHostUnifiedMemoryAllocation(testData.allocationSize, unifiedMemoryProperties);
        ASSERT_NE(testData.allocation, nullptr);
    }
    size_t expectedCacheSize = 0u;
    ASSERT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), expectedCacheSize);

    for (auto const &testData : testDataset) {
        svmManager->freeSVMAlloc(testData.allocation);
        EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), ++expectedCacheSize);
        bool foundInCache = false;
        for (auto i = 0u; i < svmManager->usmHostAllocationsCache.allocations.size(); ++i) {
            if (svmManager->usmHostAllocationsCache.allocations[i].allocation == testData.allocation) {
                foundInCache = true;
                break;
            }
        }
        EXPECT_TRUE(foundInCache);
    }
    EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), testDataset.size());

    svmManager->trimUSMHostAllocCache();
    EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 0u);
}

TEST_F(SvmHostAllocationCacheTest, givenAllocationCacheEnabledWhenInitializedThenMaxSizeIsSetCorrectly) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(2);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_TRUE(svmManager->usmHostAllocationsCacheEnabled);

    auto expectedMaxSize = static_cast<size_t>(svmManager->memoryManager->getSystemSharedMemory(mockRootDeviceIndex) * 0.02);
    EXPECT_EQ(expectedMaxSize, svmManager->usmHostAllocationsCache.maxSize);
}

TEST_F(SvmHostAllocationCacheTest, givenAllocationCacheEnabledWhenFreeingHostAllocationThenItIsPutIntoCacheOnlyIfMaxSizeWillNotBeExceeded) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = device->getMemoryManager();
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager, false);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_TRUE(svmManager->usmHostAllocationsCacheEnabled);

    constexpr auto allocationSize = MemoryConstants::pageSize64k;
    svmManager->usmHostAllocationsCache.maxSize = allocationSize;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    {
        auto allocation = svmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation, nullptr);
        auto allocation2 = svmManager->createHostUnifiedMemoryAllocation(1u, unifiedMemoryProperties);
        ASSERT_NE(allocation2, nullptr);
        EXPECT_EQ(0u, svmManager->usmHostAllocationsCache.allocations.size());
        EXPECT_EQ(0u, memoryManager->getHostAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(allocation);
        EXPECT_EQ(1u, svmManager->usmHostAllocationsCache.allocations.size());
        EXPECT_EQ(allocationSize, memoryManager->getHostAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(allocation2);
        EXPECT_EQ(1u, svmManager->usmHostAllocationsCache.allocations.size());
        EXPECT_EQ(allocationSize, memoryManager->getHostAllocationsSavedForReuseSize());

        auto recycledAllocation = svmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(recycledAllocation, allocation);
        EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 0u);
        EXPECT_EQ(0u, memoryManager->getHostAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(recycledAllocation);

        svmManager->trimUSMHostAllocCache();
        EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 0u);
        EXPECT_EQ(0u, memoryManager->getHostAllocationsSavedForReuseSize());
    }
    {
        auto allocation = svmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation, nullptr);
        auto allocation2 = svmManager->createHostUnifiedMemoryAllocation(1u, unifiedMemoryProperties);
        ASSERT_NE(allocation2, nullptr);
        EXPECT_EQ(0u, svmManager->usmHostAllocationsCache.allocations.size());
        EXPECT_EQ(0u, memoryManager->getHostAllocationsSavedForReuseSize());

        svmManager->freeSVMAllocDefer(allocation);
        EXPECT_EQ(1u, svmManager->usmHostAllocationsCache.allocations.size());
        EXPECT_EQ(allocationSize, memoryManager->getHostAllocationsSavedForReuseSize());

        svmManager->freeSVMAllocDefer(allocation2);
        EXPECT_EQ(1u, svmManager->usmHostAllocationsCache.allocations.size());
        EXPECT_EQ(allocationSize, memoryManager->getHostAllocationsSavedForReuseSize());

        auto recycledAllocation = svmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(recycledAllocation, allocation);
        EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 0u);
        EXPECT_EQ(0u, memoryManager->getHostAllocationsSavedForReuseSize());

        svmManager->freeSVMAllocDefer(recycledAllocation);

        svmManager->trimUSMHostAllocCache();
        EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 0u);
        EXPECT_EQ(0u, memoryManager->getHostAllocationsSavedForReuseSize());
    }
}

TEST_F(SvmHostAllocationCacheTest, givenAllocationCacheEnabledAndMultipleSVMManagersWhenFreeingHostAllocationThenItIsPutIntoCacheOnlyIfMaxSizeWillNotBeExceeded) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = device->getMemoryManager();
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager, false);
    auto secondSvmManager = std::make_unique<MockSVMAllocsManager>(memoryManager, false);
    svmManager->initUsmAllocationsCaches(*device);
    secondSvmManager->initUsmAllocationsCaches(*device);
    ASSERT_TRUE(svmManager->usmHostAllocationsCacheEnabled);
    ASSERT_TRUE(secondSvmManager->usmHostAllocationsCacheEnabled);

    constexpr auto allocationSize = MemoryConstants::pageSize64k;
    svmManager->usmHostAllocationsCache.maxSize = allocationSize;
    secondSvmManager->usmHostAllocationsCache.maxSize = allocationSize;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    {
        auto allocation = svmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation, nullptr);
        auto allocation2 = secondSvmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation2, nullptr);
        EXPECT_EQ(0u, svmManager->usmHostAllocationsCache.allocations.size());
        EXPECT_EQ(0u, secondSvmManager->usmHostAllocationsCache.allocations.size());
        EXPECT_EQ(0u, memoryManager->getHostAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(allocation);
        EXPECT_EQ(1u, svmManager->usmHostAllocationsCache.allocations.size());
        EXPECT_EQ(allocationSize, memoryManager->getHostAllocationsSavedForReuseSize());

        secondSvmManager->freeSVMAlloc(allocation2);
        EXPECT_EQ(0u, secondSvmManager->usmHostAllocationsCache.allocations.size());
        EXPECT_EQ(allocationSize, memoryManager->getHostAllocationsSavedForReuseSize());

        auto recycledAllocation = svmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(recycledAllocation, allocation);
        EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 0u);
        EXPECT_EQ(0u, memoryManager->getHostAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(recycledAllocation);

        svmManager->trimUSMHostAllocCache();
        EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 0u);
        EXPECT_EQ(0u, memoryManager->getHostAllocationsSavedForReuseSize());
    }
    {
        auto allocation = svmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation, nullptr);
        auto allocation2 = secondSvmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation2, nullptr);
        EXPECT_EQ(0u, svmManager->usmHostAllocationsCache.allocations.size());
        EXPECT_EQ(0u, secondSvmManager->usmHostAllocationsCache.allocations.size());
        EXPECT_EQ(0u, memoryManager->getHostAllocationsSavedForReuseSize());

        secondSvmManager->freeSVMAlloc(allocation2);
        EXPECT_EQ(1u, secondSvmManager->usmHostAllocationsCache.allocations.size());
        EXPECT_EQ(allocationSize, memoryManager->getHostAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(allocation);
        EXPECT_EQ(0u, svmManager->usmHostAllocationsCache.allocations.size());
        EXPECT_EQ(allocationSize, memoryManager->getHostAllocationsSavedForReuseSize());

        auto recycledAllocation = secondSvmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(recycledAllocation, allocation2);
        EXPECT_EQ(secondSvmManager->usmHostAllocationsCache.allocations.size(), 0u);
        EXPECT_EQ(0u, memoryManager->getHostAllocationsSavedForReuseSize());

        secondSvmManager->freeSVMAlloc(recycledAllocation);

        secondSvmManager->trimUSMHostAllocCache();
        EXPECT_EQ(secondSvmManager->usmHostAllocationsCache.allocations.size(), 0u);
        EXPECT_EQ(0u, memoryManager->getHostAllocationsSavedForReuseSize());
    }
}

TEST_F(SvmHostAllocationCacheTest, givenAllocationsWithDifferentSizesWhenAllocatingAfterFreeThenReturnCorrectCachedAllocation) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_TRUE(svmManager->usmHostAllocationsCacheEnabled);
    svmManager->usmHostAllocationsCache.maxSize = 1 * MemoryConstants::gigaByte;

    auto testDataset = std::vector<SvmHostAllocationCacheSimpleTestDataType>(
        {
            {(allocationSizeBasis * 1), nullptr},
            {(allocationSizeBasis * 1) + 1, nullptr},
            {(allocationSizeBasis * 2), nullptr},
            {(allocationSizeBasis * 2) + 1, nullptr},
            {(allocationSizeBasis * 3), nullptr},
            {(allocationSizeBasis * 3) + 1, nullptr},
        });

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    for (auto &testData : testDataset) {
        testData.allocation = svmManager->createHostUnifiedMemoryAllocation(testData.allocationSize, unifiedMemoryProperties);
        ASSERT_NE(testData.allocation, nullptr);
    }

    size_t expectedCacheSize = 0u;
    ASSERT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), expectedCacheSize);

    for (auto const &testData : testDataset) {
        svmManager->freeSVMAlloc(testData.allocation);
    }

    ASSERT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), testDataset.size());

    std::vector<void *> allocationsToFree;

    for (auto &testData : testDataset) {
        auto secondAllocation = svmManager->createHostUnifiedMemoryAllocation(testData.allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), testDataset.size() - 1);
        EXPECT_EQ(secondAllocation, testData.allocation);
        svmManager->freeSVMAlloc(secondAllocation);
        EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), testDataset.size());
    }

    svmManager->trimUSMHostAllocCache();
    EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 0u);
}

TEST_F(SvmHostAllocationCacheTest, givenAllocationsWithDifferentSizesWhenAllocatingAfterFreeThenLimitMemoryWastage) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_TRUE(svmManager->usmHostAllocationsCacheEnabled);
    svmManager->usmHostAllocationsCache.maxSize = 1 * MemoryConstants::gigaByte;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto allocation = svmManager->createHostUnifiedMemoryAllocation(SVMAllocsManager::SvmAllocationCache::minimalSizeToCheckUtilization, unifiedMemoryProperties);
    ASSERT_NE(allocation, nullptr);

    svmManager->freeSVMAlloc(allocation);

    ASSERT_EQ(1u, svmManager->usmHostAllocationsCache.allocations.size());

    constexpr auto allowedSizeForReuse = static_cast<size_t>(SVMAllocsManager::SvmAllocationCache::minimalSizeToCheckUtilization * SVMAllocsManager::SvmAllocationCache::minimalAllocUtilization);
    constexpr auto notAllowedSizeDueToMemoryWastage = allowedSizeForReuse - 1u;

    auto notReusedDueToMemoryWastage = svmManager->createHostUnifiedMemoryAllocation(notAllowedSizeDueToMemoryWastage, unifiedMemoryProperties);
    EXPECT_NE(nullptr, notReusedDueToMemoryWastage);
    EXPECT_NE(notReusedDueToMemoryWastage, allocation);
    EXPECT_EQ(1u, svmManager->usmHostAllocationsCache.allocations.size());

    auto reused = svmManager->createHostUnifiedMemoryAllocation(allowedSizeForReuse, unifiedMemoryProperties);
    EXPECT_NE(nullptr, notReusedDueToMemoryWastage);
    EXPECT_EQ(reused, allocation);
    EXPECT_EQ(0u, svmManager->usmHostAllocationsCache.allocations.size());

    svmManager->freeSVMAlloc(notReusedDueToMemoryWastage);
    svmManager->freeSVMAlloc(reused);
    svmManager->trimUSMHostAllocCache();
    EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 0u);
}

TEST_F(SvmHostAllocationCacheTest, givenAllocationOverSizeLimitWhenAllocatingAfterFreeThenDontSaveForReuse) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_TRUE(svmManager->usmHostAllocationsCacheEnabled);
    svmManager->usmHostAllocationsCache.maxSize = 1 * MemoryConstants::gigaByte;
    const auto notAcceptedAllocSize = SVMAllocsManager::SvmAllocationCache::maxServicedSize + 1;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto mockMemoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    auto mockGa = std::make_unique<MockGraphicsAllocation>(mockRootDeviceIndex, nullptr, notAcceptedAllocSize);
    mockGa->gpuAddress = 0xbadf00;
    mockGa->cpuPtr = reinterpret_cast<void *>(0xbadf00);
    mockGa->setAllocationType(AllocationType::svmCpu);
    mockMemoryManager->mockGa = mockGa.release();
    mockMemoryManager->returnMockGAFromHostPool = true;
    auto allocation = svmManager->createHostUnifiedMemoryAllocation(notAcceptedAllocSize, unifiedMemoryProperties);
    EXPECT_EQ(reinterpret_cast<void *>(0xbadf00), allocation);

    svmManager->freeSVMAlloc(allocation);

    EXPECT_EQ(0u, svmManager->usmHostAllocationsCache.allocations.size());
}

TEST_F(SvmHostAllocationCacheTest, givenMultipleAllocationsWhenAllocatingAfterFreeThenReturnAllocationsInCacheStartingFromSmallest) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_TRUE(svmManager->usmHostAllocationsCacheEnabled);
    svmManager->usmHostAllocationsCache.maxSize = 1 * MemoryConstants::gigaByte;

    auto testDataset = std::vector<SvmHostAllocationCacheSimpleTestDataType>(
        {
            {(allocationSizeBasis * 1), nullptr},
            {(allocationSizeBasis * 2), nullptr},
            {(allocationSizeBasis * 3), nullptr},
        });

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    for (auto &testData : testDataset) {
        testData.allocation = svmManager->createHostUnifiedMemoryAllocation(testData.allocationSize, unifiedMemoryProperties);
        ASSERT_NE(testData.allocation, nullptr);
    }

    ASSERT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 0u);

    for (auto const &testData : testDataset) {
        svmManager->freeSVMAlloc(testData.allocation);
    }

    size_t expectedCacheSize = testDataset.size();
    ASSERT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), expectedCacheSize);

    auto allocationLargerThanInCache = svmManager->createHostUnifiedMemoryAllocation(allocationSizeBasis << 3, unifiedMemoryProperties);
    EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), expectedCacheSize);

    auto firstAllocation = svmManager->createHostUnifiedMemoryAllocation(allocationSizeBasis, unifiedMemoryProperties);
    EXPECT_EQ(firstAllocation, testDataset[0].allocation);
    EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), --expectedCacheSize);

    auto secondAllocation = svmManager->createHostUnifiedMemoryAllocation(allocationSizeBasis, unifiedMemoryProperties);
    EXPECT_EQ(secondAllocation, testDataset[1].allocation);
    EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), --expectedCacheSize);

    auto thirdAllocation = svmManager->createHostUnifiedMemoryAllocation(allocationSizeBasis, unifiedMemoryProperties);
    EXPECT_EQ(thirdAllocation, testDataset[2].allocation);
    EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 0u);

    svmManager->freeSVMAlloc(firstAllocation);
    svmManager->freeSVMAlloc(secondAllocation);
    svmManager->freeSVMAlloc(thirdAllocation);
    svmManager->freeSVMAlloc(allocationLargerThanInCache);

    svmManager->trimUSMHostAllocCache();
    EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 0u);
}

struct SvmHostAllocationCacheTestDataType {
    SvmHostAllocationCacheTestDataType(size_t allocationSize,
                                       const RootDeviceIndicesContainer &rootDeviceIndicesArg,
                                       std::map<uint32_t, DeviceBitfield> &subdeviceBitFields,
                                       Device *device,
                                       std::string name) : allocationSize(allocationSize),
                                                           unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory,
                                                                                   1,
                                                                                   rootDeviceIndicesArg,
                                                                                   subdeviceBitFields),
                                                           name(name){

                                                           };
    size_t allocationSize;
    void *allocation{nullptr};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties;
    std::string name;
};

TEST_F(SvmHostAllocationCacheTest, givenAllocationsWithDifferentFlagsWhenAllocatingAfterFreeThenReturnCorrectAllocation) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto rootDevice = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(rootDevice->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*rootDevice);
    ASSERT_TRUE(svmManager->usmHostAllocationsCacheEnabled);
    svmManager->usmHostAllocationsCache.maxSize = 1 * MemoryConstants::gigaByte;

    size_t defaultAllocSize = allocationSizeBasis;
    std::map<uint32_t, DeviceBitfield> subDeviceBitfields = {{0u, rootDevice->getDeviceBitfield()}};
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDevice->getRootDeviceIndex());
    SvmHostAllocationCacheTestDataType
        defaultAlloc(defaultAllocSize,
                     rootDeviceIndices,
                     subDeviceBitfields,
                     rootDevice, "defaultAlloc"),
        writeOnly(defaultAllocSize,
                  rootDeviceIndices,
                  subDeviceBitfields,
                  rootDevice, "writeOnly"),
        readOnly(defaultAllocSize,
                 rootDeviceIndices,
                 subDeviceBitfields,
                 rootDevice, "readOnly"),
        allocWriteCombined(defaultAllocSize,
                           rootDeviceIndices,
                           subDeviceBitfields,
                           rootDevice, "allocWriteCombined");
    writeOnly.unifiedMemoryProperties.allocationFlags.flags.writeOnly = true;
    readOnly.unifiedMemoryProperties.allocationFlags.flags.readOnly = true;
    allocWriteCombined.unifiedMemoryProperties.allocationFlags.allocFlags.allocWriteCombined = true;

    auto testDataset = std::vector<SvmHostAllocationCacheTestDataType>({defaultAlloc, writeOnly, readOnly, allocWriteCombined});
    for (auto &allocationDataToVerify : testDataset) {

        for (auto &testData : testDataset) {
            testData.allocation = svmManager->createHostUnifiedMemoryAllocation(testData.allocationSize, testData.unifiedMemoryProperties);
        }
        ASSERT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 0u);

        for (auto &testData : testDataset) {
            svmManager->freeSVMAlloc(testData.allocation);
        }
        ASSERT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), testDataset.size());

        auto allocationFromCache = svmManager->createHostUnifiedMemoryAllocation(allocationDataToVerify.allocationSize, allocationDataToVerify.unifiedMemoryProperties);
        EXPECT_EQ(allocationFromCache, allocationDataToVerify.allocation);

        auto allocationNotFromCache = svmManager->createHostUnifiedMemoryAllocation(allocationDataToVerify.allocationSize, allocationDataToVerify.unifiedMemoryProperties);
        for (auto &cachedAllocation : testDataset) {
            EXPECT_NE(allocationNotFromCache, cachedAllocation.allocation);
        }
        svmManager->freeSVMAlloc(allocationFromCache);
        svmManager->freeSVMAlloc(allocationNotFromCache);

        svmManager->trimUSMHostAllocCache();
        ASSERT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 0u);
    }
}

TEST_F(SvmHostAllocationCacheTest, givenHostOutOfMemoryWhenAllocatingThenCacheIsTrimmedAndAllocationSucceeds) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    device->injectMemoryManager(new MockMemoryManagerWithCapacity(*device->getExecutionEnvironment()));
    MockMemoryManagerWithCapacity *memoryManager = static_cast<MockMemoryManagerWithCapacity *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager, false);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_TRUE(svmManager->usmHostAllocationsCacheEnabled);
    svmManager->usmHostAllocationsCache.maxSize = 1 * MemoryConstants::gigaByte;

    memoryManager->capacity = MemoryConstants::pageSize64k * 3;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto allocationInCache = svmManager->createHostUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties);
    auto allocationInCache2 = svmManager->createHostUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties);
    auto allocationInCache3 = svmManager->createHostUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties);
    ASSERT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 0u);
    svmManager->freeSVMAlloc(allocationInCache);
    svmManager->freeSVMAlloc(allocationInCache2);
    svmManager->freeSVMAllocDefer(allocationInCache3);

    ASSERT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 3u);
    ASSERT_NE(svmManager->getSVMAlloc(allocationInCache), nullptr);
    ASSERT_NE(svmManager->getSVMAlloc(allocationInCache2), nullptr);
    ASSERT_NE(svmManager->getSVMAlloc(allocationInCache3), nullptr);
    auto ptr = svmManager->createHostUnifiedMemoryAllocation(MemoryConstants::pageSize64k * 2, unifiedMemoryProperties);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 0u);
    svmManager->freeSVMAlloc(ptr);

    svmManager->trimUSMHostAllocCache();
    ASSERT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 0u);
}

TEST_F(SvmHostAllocationCacheTest, givenAllocationInUsageWhenAllocatingAfterFreeThenDoNotReuseAllocation) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    svmManager->initUsmAllocationsCaches(*device);
    EXPECT_TRUE(svmManager->usmHostAllocationsCacheEnabled);
    svmManager->usmHostAllocationsCache.maxSize = 1 * MemoryConstants::gigaByte;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto allocation = svmManager->createUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    EXPECT_NE(allocation, nullptr);
    svmManager->freeSVMAlloc(allocation);
    EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 1u);

    MockMemoryManager *mockMemoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    mockMemoryManager->deferAllocInUse = true;
    auto testedAllocation = svmManager->createUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 1u);
    auto svmData = svmManager->getSVMAlloc(testedAllocation);
    EXPECT_NE(nullptr, svmData);

    svmManager->freeSVMAlloc(testedAllocation);
    EXPECT_EQ(svmManager->usmHostAllocationsCache.allocations.size(), 2u);

    svmManager->trimUSMHostAllocCache();
}
} // namespace NEO
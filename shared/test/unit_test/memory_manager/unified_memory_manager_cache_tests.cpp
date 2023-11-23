/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

using namespace NEO;

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
        tracker.insert(data);
    }
    for (uint32_t i = 0; i < graphicsAllocationsSize / 2; ++i) {
        data.gpuAllocations.addAllocation(&graphicsAllocations[i]);
        data.device = reinterpret_cast<Device *>(graphicsAllocations[i].getGpuAddress());
        tracker.insert(data);
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
    tracker.insert(data);

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
    tracker.remove(*data2);
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

TEST(SvmDeviceAllocationCacheTest, givenAllocationCacheDefaultWhenCheckingIfEnabledThenItIsDisabled) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    ASSERT_EQ(DebugManager.flags.ExperimentalEnableDeviceAllocationCache.get(), -1);
    EXPECT_FALSE(svmManager->usmDeviceAllocationsCacheEnabled);
}

struct SvmDeviceAllocationCacheSimpleTestDataType {
    size_t allocationSize;
    void *allocation;
};

TEST(SvmDeviceAllocationCacheTest, givenAllocationCacheEnabledWhenFreeingDeviceAllocationThenItIsPutIntoCache) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    DebugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    ASSERT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);

    constexpr auto allocationSizeBasis = MemoryConstants::pageSize64k;
    auto testDataset = std::vector<SvmDeviceAllocationCacheSimpleTestDataType>(
        {{1u, nullptr},
         {(allocationSizeBasis << 0) - 1, nullptr},
         {(allocationSizeBasis << 0), nullptr},
         {(allocationSizeBasis << 0) + 1, nullptr},
         {(allocationSizeBasis << 1) - 1, nullptr},
         {(allocationSizeBasis << 1), nullptr},
         {(allocationSizeBasis << 1) + 1, nullptr}});

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY, 1, rootDeviceIndices, deviceBitfields);
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

TEST(SvmDeviceAllocationCacheTest, givenAllocationsWithDifferentSizesWhenAllocatingAfterFreeThenReturnCorrectCachedAllocation) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    DebugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    ASSERT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);

    constexpr auto allocationSizeBasis = MemoryConstants::pageSize64k;
    auto testDataset = std::vector<SvmDeviceAllocationCacheSimpleTestDataType>(
        {
            {(allocationSizeBasis << 0), nullptr},
            {(allocationSizeBasis << 0) + 1, nullptr},
            {(allocationSizeBasis << 1), nullptr},
            {(allocationSizeBasis << 1) + 1, nullptr},
            {(allocationSizeBasis << 2), nullptr},
            {(allocationSizeBasis << 2) + 1, nullptr},
        });

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY, 1, rootDeviceIndices, deviceBitfields);
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

TEST(SvmDeviceAllocationCacheTest, givenMultipleAllocationsWhenAllocatingAfterFreeThenReturnAllocationsInCacheStartingFromSmallest) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    DebugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    ASSERT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);

    size_t allocationSizeBasis = MemoryConstants::pageSize64k;
    auto testDataset = std::vector<SvmDeviceAllocationCacheSimpleTestDataType>(
        {
            {(allocationSizeBasis << 0), nullptr},
            {(allocationSizeBasis << 1), nullptr},
            {(allocationSizeBasis << 2), nullptr},
        });

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY, 1, rootDeviceIndices, deviceBitfields);
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
                                                             unifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY,
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

TEST(SvmDeviceAllocationCacheTest, givenAllocationsWithDifferentFlagsWhenAllocatingAfterFreeThenReturnCorrectAllocation) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(2, 2));
    DebugManagerStateRestore restore;
    DebugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto rootDevice = deviceFactory->rootDevices[0];
    auto secondRootDevice = deviceFactory->rootDevices[1];
    auto subDevice1 = deviceFactory->subDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(rootDevice->getMemoryManager(), false);
    ASSERT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);

    constexpr auto allocationSizeBasis = MemoryConstants::kiloByte;
    size_t defaultAllocSize = allocationSizeBasis << 0;
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

TEST(SvmDeviceAllocationCacheTest, givenDeviceOutOfMemoryWhenAllocatingThenCacheIsTrimmedAndAllocationSucceeds) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    DebugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    device->injectMemoryManager(new MockMemoryManagerWithCapacity(*device->getExecutionEnvironment()));
    MockMemoryManagerWithCapacity *memoryManager = static_cast<MockMemoryManagerWithCapacity *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager, false);
    ASSERT_TRUE(svmManager->usmDeviceAllocationsCacheEnabled);

    memoryManager->capacity = MemoryConstants::pageSize64k * 3;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY, 1, rootDeviceIndices, deviceBitfields);
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

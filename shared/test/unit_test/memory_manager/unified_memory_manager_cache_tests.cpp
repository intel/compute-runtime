/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/memory_manager/unified_memory_reuse_cleaner.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/mocks/mock_ail_configuration.h"
#include "shared/test/common/mocks/mock_debugger.h"
#include "shared/test/common/mocks/mock_deferred_deleter.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/mock_usm_memory_reuse_cleaner.h"
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
    using SvmAllocationCache = SVMAllocsManager::SvmAllocationCache;
    static constexpr size_t allocationSizeBasis = SvmAllocationCache::minimalSizeToCheckUtilization;
    EXPECT_TRUE(SvmAllocationCache::allocUtilizationAllows(1u, allocationSizeBasis - 1));
    EXPECT_TRUE(SvmAllocationCache::allocUtilizationAllows(allocationSizeBasis - 1, allocationSizeBasis - 1));
    EXPECT_FALSE(SvmAllocationCache::allocUtilizationAllows(allocationSizeBasis / 2 - 1, allocationSizeBasis));
    EXPECT_TRUE(SvmAllocationCache::allocUtilizationAllows(allocationSizeBasis / 2, allocationSizeBasis));
    EXPECT_TRUE(SvmAllocationCache::allocUtilizationAllows(allocationSizeBasis, allocationSizeBasis));
}

TEST(SvmAllocationCacheSimpleTest, givenDifferentAlignmentsWhenCheckingIfAlignmentAllowedThenReturnCorrectValue) {
    using SvmAllocationCache = SVMAllocsManager::SvmAllocationCache;
    void *ptr = addrToPtr(512u);
    EXPECT_TRUE(SvmAllocationCache::alignmentAllows(ptr, 0u));
    EXPECT_TRUE(SvmAllocationCache::alignmentAllows(ptr, 256u));
    EXPECT_TRUE(SvmAllocationCache::alignmentAllows(ptr, 512u));
    EXPECT_FALSE(SvmAllocationCache::alignmentAllows(ptr, 1024u));
}

TEST(SvmAllocationCacheSimpleTest, givenDifferentSizesWhenCheckingIfSizeAllowsThenReturnCorrectValue) {
    EXPECT_TRUE(SVMAllocsManager::SvmAllocationCache::sizeAllowed(256 * MemoryConstants::megaByte));
    EXPECT_FALSE(SVMAllocsManager::SvmAllocationCache::sizeAllowed(256 * MemoryConstants::megaByte + 1));
}

TEST(SvmAllocationCacheSimpleTest, givenSvmAllocationCacheInfoWhenMarkedForDeleteThenSetSizeToZero) {
    SVMAllocsManager::SvmCacheAllocationInfo info(MemoryConstants::pageSize64k, nullptr, nullptr, false);
    EXPECT_FALSE(SVMAllocsManager::SvmCacheAllocationInfo::isMarkedForDelete(info));
    info.markForDelete();
    EXPECT_EQ(0u, info.allocationSize);
    EXPECT_TRUE(SVMAllocsManager::SvmCacheAllocationInfo::isMarkedForDelete(info));
}

TEST(SvmAllocationCacheSimpleTest, givenAllocationsWhenCheckingIsInUseThenReturnCorrectValue) {
    using SvmCacheAllocationInfo = SVMAllocsManager::SvmCacheAllocationInfo;
    SVMAllocsManager::SvmAllocationCache allocationCache;
    MockMemoryManager memoryManager;
    MockSVMAllocsManager svmAllocsManager(&memoryManager);

    allocationCache.memoryManager = &memoryManager;
    allocationCache.svmAllocsManager = &svmAllocsManager;

    {
        constexpr bool completed = false;
        memoryManager.deferAllocInUse = false;
        MockGraphicsAllocation gpuGfxAllocation;
        SvmAllocationData svmAllocData(mockRootDeviceIndex);
        SvmCacheAllocationInfo svmCacheAllocInfo(1u, addrToPtr(0xFULL), &svmAllocData, completed);
        EXPECT_FALSE(allocationCache.isInUse(svmCacheAllocInfo));
        svmAllocData.gpuAllocations.addAllocation(&gpuGfxAllocation);
        EXPECT_FALSE(allocationCache.isInUse(svmCacheAllocInfo));
        memoryManager.deferAllocInUse = true;
        EXPECT_TRUE(allocationCache.isInUse(svmCacheAllocInfo));
    }
    {
        constexpr bool completed = false;
        memoryManager.deferAllocInUse = false;
        MockGraphicsAllocation cpuGfxAllocation;
        SvmAllocationData svmAllocData(mockRootDeviceIndex);
        svmAllocData.cpuAllocation = &cpuGfxAllocation;
        SvmCacheAllocationInfo svmCacheAllocInfo(1u, addrToPtr(0xFULL), &svmAllocData, completed);
        EXPECT_FALSE(allocationCache.isInUse(svmCacheAllocInfo));
        memoryManager.deferAllocInUse = true;
        EXPECT_TRUE(allocationCache.isInUse(svmCacheAllocInfo));
    }
    {
        constexpr bool completed = true;
        memoryManager.deferAllocInUse = false;
        MockGraphicsAllocation cpuGfxAllocation;
        SvmAllocationData svmAllocData(mockRootDeviceIndex);
        svmAllocData.cpuAllocation = &cpuGfxAllocation;
        SvmCacheAllocationInfo svmCacheAllocInfo(1u, addrToPtr(0xFULL), &svmAllocData, completed);
        EXPECT_FALSE(allocationCache.isInUse(svmCacheAllocInfo));
        memoryManager.deferAllocInUse = true;
        EXPECT_FALSE(allocationCache.isInUse(svmCacheAllocInfo));
    }
}

TEST(SvmAllocationCacheSimpleTest, givenAllocationsWhenInsertingAllocationThenDoNotInsertImportedNorInternal) {
    SVMAllocsManager::SvmAllocationCache allocationCache;
    MockMemoryManager memoryManager;
    MockSVMAllocsManager svmAllocsManager(&memoryManager);

    allocationCache.memoryManager = &memoryManager;
    allocationCache.svmAllocsManager = &svmAllocsManager;
    memoryManager.usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);

    void *ptr = addrToPtr(0xFULL);
    MockGraphicsAllocation gpuGfxAllocation;
    SvmAllocationData svmAllocData(mockRootDeviceIndex);
    svmAllocData.gpuAllocations.addAllocation(&gpuGfxAllocation);
    {
        svmAllocData.isImportedAllocation = false;
        svmAllocData.isInternalAllocation = false;
        EXPECT_TRUE(allocationCache.insert(1u, ptr, &svmAllocData, false));
        allocationCache.allocations.clear();
    }
    {
        svmAllocData.isImportedAllocation = true;
        svmAllocData.isInternalAllocation = false;
        EXPECT_FALSE(allocationCache.insert(1u, ptr, &svmAllocData, false));
        allocationCache.allocations.clear();
    }
    {
        svmAllocData.isImportedAllocation = false;
        svmAllocData.isInternalAllocation = true;
        EXPECT_FALSE(allocationCache.insert(1u, ptr, &svmAllocData, false));
        allocationCache.allocations.clear();
    }
}

TEST(SvmAllocationCacheSimpleTest, givenAllocationsWhenGettingAllocationThenUpdateAllocIdIfBoolIsSet) {
    SVMAllocsManager::SvmAllocationCache allocationCache;
    MockMemoryManager memoryManager;
    MockSVMAllocsManager svmAllocsManager(&memoryManager);
    svmAllocsManager.allocationsCounter.store(1u);

    allocationCache.memoryManager = &memoryManager;
    allocationCache.svmAllocsManager = &svmAllocsManager;
    memoryManager.usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);

    void *ptr = addrToPtr(0xFULL);
    MockGraphicsAllocation gpuGfxAllocation;
    SvmAllocationData svmAllocData(mockRootDeviceIndex);
    svmAllocData.gpuAllocations.addAllocation(&gpuGfxAllocation);
    svmAllocData.setAllocId(1u);
    svmAllocsManager.insertSVMAlloc(ptr, svmAllocData);

    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    {
        allocationCache.requireUpdatingAllocsForIndirectAccess = false;
        EXPECT_EQ(1u, svmAllocsManager.internalAllocationsMap.count(1u));
        EXPECT_TRUE(allocationCache.insert(1u, ptr, &svmAllocData, false));
        EXPECT_EQ(1u, svmAllocsManager.internalAllocationsMap.count(1u));
        auto reusedPtr = allocationCache.get(1u, unifiedMemoryProperties);
        EXPECT_EQ(ptr, reusedPtr);
        EXPECT_EQ(1u, svmAllocData.getAllocId());
        EXPECT_EQ(1u, svmAllocsManager.allocationsCounter.load());
        EXPECT_EQ(1u, svmAllocsManager.internalAllocationsMap.count(1u));
        auto allocMapEntry = svmAllocsManager.internalAllocationsMap.find(1u);
        EXPECT_EQ(&gpuGfxAllocation, allocMapEntry->second);
        EXPECT_EQ(0u, svmAllocsManager.internalAllocationsMap.count(2u));
        allocationCache.allocations.clear();
        svmAllocsManager.internalAllocationsMap.clear();
    }

    {
        svmAllocsManager.internalAllocationsMap.insert({svmAllocData.getAllocId(), &gpuGfxAllocation});
        EXPECT_EQ(1u, svmAllocsManager.internalAllocationsMap.count(1u));
        EXPECT_EQ(0u, svmAllocsManager.internalAllocationsMap.count(2u));
        allocationCache.requireUpdatingAllocsForIndirectAccess = true;
        EXPECT_TRUE(allocationCache.insert(1u, ptr, &svmAllocData, false));
        EXPECT_EQ(0u, svmAllocsManager.internalAllocationsMap.count(1u));
        EXPECT_EQ(0u, svmAllocsManager.internalAllocationsMap.count(2u));
        auto reusedPtr = allocationCache.get(1u, unifiedMemoryProperties);
        EXPECT_EQ(ptr, reusedPtr);
        EXPECT_EQ(2u, svmAllocData.getAllocId());
        EXPECT_EQ(2u, svmAllocsManager.allocationsCounter.load());
        EXPECT_EQ(0u, svmAllocsManager.internalAllocationsMap.count(1u));
        EXPECT_EQ(1u, svmAllocsManager.internalAllocationsMap.count(2u));
        auto allocMapEntry = svmAllocsManager.internalAllocationsMap.find(2u);
        EXPECT_EQ(&gpuGfxAllocation, allocMapEntry->second);
        allocationCache.allocations.clear();
    }
}

struct SvmAllocationCacheTestFixture {
    SvmAllocationCacheTestFixture() : executionEnvironment(defaultHwInfo.get()) {}
    void setUp() {
    }
    void tearDown() {
    }
    static constexpr size_t allocationSizeBasis = MemoryConstants::pageSize64k;
    static constexpr uint64_t alwaysLimited = 0u;
    MockExecutionEnvironment executionEnvironment;
};

using SvmDeviceAllocationCacheTest = Test<SvmAllocationCacheTestFixture>;

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationCacheDisabledWhenCheckingIfEnabledThenItIsDisabled) {
    DebugManagerStateRestore restorer;
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    auto device = deviceFactory->rootDevices[0];
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(0);
    device->initUsmReuseLimits();
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    EXPECT_EQ(nullptr, svmManager->usmDeviceAllocationsCache);
    svmManager->initUsmAllocationsCaches(*device);
    EXPECT_EQ(nullptr, svmManager->usmDeviceAllocationsCache);
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationCacheEnabledAndMaxSizeZeroWhenCallingCleanupThenNoError) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(0);
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    device->usmReuseInfo.init(0, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    EXPECT_EQ(nullptr, svmManager->usmDeviceAllocationsCache);
    svmManager->cleanupUSMAllocCaches();
}

HWTEST_F(SvmDeviceAllocationCacheTest, givenOclApiSpecificConfigAndProductHelperAndDebuggerWhenCheckingIfEnabledThenEnableCorrectly) {
    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    MockDevice *device = deviceFactory->rootDevices[0];
    RAIIProductHelperFactory<MockProductHelper> raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    MockAILConfiguration mockAilConfigurationHelper;
    device->mockAilConfigurationHelper = &mockAilConfigurationHelper;
    raii.mockProductHelper->isHostUsmAllocationReuseSupportedResult = false;
    {
        raii.mockProductHelper->isDeviceUsmAllocationReuseSupportedResult = false;
        device->initUsmReuseLimits();
        EXPECT_EQ(0u, device->usmReuseInfo.getMaxAllocationsSavedForReuseSize());
        auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
        EXPECT_EQ(nullptr, svmManager->usmDeviceAllocationsCache);
        svmManager->initUsmAllocationsCaches(*device);
        EXPECT_EQ(nullptr, svmManager->usmDeviceAllocationsCache);
    }
    {
        raii.mockProductHelper->isDeviceUsmAllocationReuseSupportedResult = true;
        device->initUsmReuseLimits();
        auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
        EXPECT_EQ(nullptr, svmManager->usmDeviceAllocationsCache);
        svmManager->initUsmAllocationsCaches(*device);
        EXPECT_NE(nullptr, svmManager->usmDeviceAllocationsCache);
        const auto expectedMaxSize = static_cast<size_t>(0.08 * device->getGlobalMemorySize(static_cast<uint32_t>(device->getDeviceBitfield().to_ullong())));
        EXPECT_EQ(expectedMaxSize, device->usmReuseInfo.getMaxAllocationsSavedForReuseSize());
    }
    {
        raii.mockProductHelper->isDeviceUsmAllocationReuseSupportedResult = true;
        mockAilConfigurationHelper.limitAmountOfDeviceMemoryForRecyclingReturn = true;
        device->initUsmReuseLimits();
        auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
        EXPECT_EQ(nullptr, svmManager->usmDeviceAllocationsCache);
        svmManager->initUsmAllocationsCaches(*device);
        EXPECT_EQ(nullptr, svmManager->usmDeviceAllocationsCache);
        EXPECT_EQ(0u, device->usmReuseInfo.getMaxAllocationsSavedForReuseSize());
    }
    {
        device->getRootDeviceEnvironmentRef().debugger.reset(new MockDebugger);
        raii.mockProductHelper->isDeviceUsmAllocationReuseSupportedResult = true;
        mockAilConfigurationHelper.limitAmountOfDeviceMemoryForRecyclingReturn = false;
        device->initUsmReuseLimits();
        auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
        EXPECT_EQ(nullptr, svmManager->usmDeviceAllocationsCache);
        svmManager->initUsmAllocationsCaches(*device);
        EXPECT_EQ(nullptr, svmManager->usmDeviceAllocationsCache);

        device->getRootDeviceEnvironmentRef().debugger.reset(nullptr);
        svmManager->initUsmAllocationsCaches(*device);
        EXPECT_NE(nullptr, svmManager->usmDeviceAllocationsCache);
    }

    for (int32_t csrType = 0;
         csrType < static_cast<int32_t>(CommandStreamReceiverType::typesNum);
         ++csrType) {
        DebugManagerStateRestore restorer;
        debugManager.flags.SetCommandStreamReceiver.set(csrType);
        debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(8);
        raii.mockProductHelper->isDeviceUsmAllocationReuseSupportedResult = true;
        mockAilConfigurationHelper.limitAmountOfDeviceMemoryForRecyclingReturn = false;
        device->initUsmReuseLimits();
        auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
        EXPECT_EQ(nullptr, svmManager->usmDeviceAllocationsCache);
        svmManager->initUsmAllocationsCaches(*device);
        ASSERT_NE(nullptr, svmManager->usmDeviceAllocationsCache);
        EXPECT_EQ(csrType != 0, svmManager->usmDeviceAllocationsCache->requireUpdatingAllocsForIndirectAccess);
    }

    for (int32_t csrType = 0;
         csrType < static_cast<int32_t>(CommandStreamReceiverType::typesNum);
         ++csrType) {
        DebugManagerStateRestore restorer;
        debugManager.flags.SetCommandStreamReceiver.set(csrType);
        raii.mockProductHelper->isDeviceUsmAllocationReuseSupportedResult = true;
        mockAilConfigurationHelper.limitAmountOfDeviceMemoryForRecyclingReturn = false;
        device->initUsmReuseLimits();
        auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
        EXPECT_EQ(nullptr, svmManager->usmDeviceAllocationsCache);
        svmManager->initUsmAllocationsCaches(*device);
        EXPECT_EQ(NEO::DeviceFactory::isHwModeSelected(), nullptr != svmManager->usmDeviceAllocationsCache);
    }
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationCacheEnabledWhenDirectSubmissionLightActiveThenCleanerDisabled) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    device->initUsmReuseLimits();
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    device->anyDirectSubmissionEnabledReturnValue = true;

    svmManager->initUsmAllocationsCaches(*device);

    ASSERT_NE(nullptr, svmManager->usmDeviceAllocationsCache);
    EXPECT_FALSE(device->getExecutionEnvironment()->unifiedMemoryReuseCleaner.get());
}

TEST_F(SvmDeviceAllocationCacheTest, givenReuseLimitFlagWhenInitUsmReuseLimitCalledThenLimitThresholdSetCorrectly) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    auto device = deviceFactory->rootDevices[0];

    {
        DebugManagerStateRestore restore;
        debugManager.flags.ExperimentalUSMAllocationReuseLimitThreshold.set(0);
        device->initUsmReuseLimits();
        EXPECT_EQ(UsmReuseInfo::notLimited, device->usmReuseInfo.getLimitAllocationsReuseThreshold());
    }
    {
        DebugManagerStateRestore restore;
        debugManager.flags.ExperimentalUSMAllocationReuseLimitThreshold.set(70);
        device->initUsmReuseLimits();
        const auto totalDeviceMemory = device->getGlobalMemorySize(static_cast<uint32_t>(device->getDeviceBitfield().to_ulong()));
        const auto expectedLimitThreshold = static_cast<uint64_t>(0.7 * totalDeviceMemory);
        EXPECT_EQ(expectedLimitThreshold, device->usmReuseInfo.getLimitAllocationsReuseThreshold());
    }
    {
        DebugManagerStateRestore restore;
        debugManager.flags.ExperimentalUSMAllocationReuseLimitThreshold.set(-1);
        device->initUsmReuseLimits();
        const auto totalDeviceMemory = device->getGlobalMemorySize(static_cast<uint32_t>(device->getDeviceBitfield().to_ulong()));
        const auto expectedLimitThreshold = static_cast<uint64_t>(0.8 * totalDeviceMemory);
        EXPECT_EQ(expectedLimitThreshold, device->usmReuseInfo.getLimitAllocationsReuseThreshold());
    }
}

struct SvmDeviceAllocationCacheSimpleTestDataType {
    size_t allocationSize;
    void *allocation;
};

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationCacheEnabledWhenFreeingDeviceAllocationThenItIsPutIntoCache) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    device->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmDeviceAllocationsCache);

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
    ASSERT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), expectedCacheSize);

    for (auto const &testData : testDataset) {
        svmManager->freeSVMAlloc(testData.allocation);
        EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), ++expectedCacheSize);
        bool foundInCache = false;
        for (auto i = 0u; i < svmManager->usmDeviceAllocationsCache->allocations.size(); ++i) {
            if (svmManager->usmDeviceAllocationsCache->allocations[i].allocation == testData.allocation) {
                foundInCache = true;
                auto svmData = svmManager->getSVMAlloc(testData.allocation);
                EXPECT_NE(nullptr, svmData);
                EXPECT_EQ(svmData, svmManager->usmDeviceAllocationsCache->allocations[i].svmData);
                EXPECT_EQ(svmData->gpuAllocations.getDefaultGraphicsAllocation()->getUnderlyingBufferSize(),
                          svmManager->usmDeviceAllocationsCache->allocations[i].allocationSize);
                break;
            }
        }
        EXPECT_TRUE(foundInCache);
    }
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), testDataset.size());

    svmManager->cleanupUSMAllocCaches();
    EXPECT_EQ(nullptr, svmManager->usmDeviceAllocationsCache);
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationCacheEnabledWhenFreeingSamePtrMultipleTimesThenNoError) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    device->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmDeviceAllocationsCache);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    auto allocation = svmManager->createUnifiedMemoryAllocation(allocationSizeBasis, unifiedMemoryProperties);
    ASSERT_NE(allocation, nullptr);
    auto svmData = svmManager->getSVMAlloc(allocation);

    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 0u);
    EXPECT_FALSE(svmData->isSavedForReuse);
    svmManager->freeSVMAlloc(allocation);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 1u);
    EXPECT_TRUE(svmData->isSavedForReuse);
    svmManager->freeSVMAlloc(allocation);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 1u);
    EXPECT_TRUE(svmData->isSavedForReuse);

    svmManager->cleanupUSMAllocCaches();
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationCacheEnabledWhenRequestingSpecificAlignmentThenCheckIfAllocationIsAligned) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    memoryManager->usmReuseInfo.init(0u, UsmReuseInfo::notLimited);
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    device->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmDeviceAllocationsCache);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    auto mockGa = std::make_unique<MockGraphicsAllocation>(mockRootDeviceIndex, nullptr, allocationSizeBasis);
    mockGa->gpuAddress = 0xff0000;
    mockGa->cpuPtr = reinterpret_cast<void *>(0xff0000);
    mockGa->setAllocationType(AllocationType::svmGpu);
    memoryManager->mockGa = mockGa.release();
    memoryManager->returnMockGAFromDevicePool = true;
    auto allocation = svmManager->createUnifiedMemoryAllocation(allocationSizeBasis, unifiedMemoryProperties);
    memoryManager->returnMockGAFromDevicePool = false;
    EXPECT_EQ(reinterpret_cast<void *>(0xff0000), allocation);

    svmManager->freeSVMAlloc(allocation);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 1u);

    unifiedMemoryProperties.alignment = MemoryConstants::pageSize2M;
    EXPECT_FALSE(SVMAllocsManager::SvmAllocationCache::alignmentAllows(allocation, unifiedMemoryProperties.alignment));
    auto differentAlignmentAlloc = svmManager->createUnifiedMemoryAllocation(allocationSizeBasis, unifiedMemoryProperties);
    EXPECT_NE(differentAlignmentAlloc, allocation);
    EXPECT_EQ(1u, svmManager->usmDeviceAllocationsCache->allocations.size());
    svmManager->freeSVMAlloc(differentAlignmentAlloc);

    svmManager->cleanupUSMAllocCaches();
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationCacheEnabledWhenInitializedThenMaxSizeIsSetCorrectly) {
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(2);
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    auto device = deviceFactory->rootDevices[0];
    device->initUsmReuseLimits();
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmDeviceAllocationsCache);

    auto expectedMaxSize = static_cast<uint64_t>(device->getGlobalMemorySize(static_cast<uint32_t>(mockDeviceBitfield.to_ulong())) * 0.02);
    EXPECT_EQ(expectedMaxSize, device->usmReuseInfo.getMaxAllocationsSavedForReuseSize());
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationCacheEnabledWhenFreeingDeviceAllocationThenItIsPutIntoCacheOnlyIfMaxSizeWillNotBeExceeded) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    constexpr auto allocationSize = MemoryConstants::pageSize64k;
    device->usmReuseInfo.init(allocationSize, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmDeviceAllocationsCache);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    {
        auto allocation = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation, nullptr);
        auto allocation2 = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation2, nullptr);
        EXPECT_EQ(0u, svmManager->usmDeviceAllocationsCache->allocations.size());
        EXPECT_EQ(0u, device->usmReuseInfo.getAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(allocation);
        EXPECT_EQ(1u, svmManager->usmDeviceAllocationsCache->allocations.size());
        EXPECT_EQ(allocationSize, device->usmReuseInfo.getAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(allocation2);
        EXPECT_EQ(1u, svmManager->usmDeviceAllocationsCache->allocations.size());
        EXPECT_EQ(allocationSize, device->usmReuseInfo.getAllocationsSavedForReuseSize());

        auto recycledAllocation = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(recycledAllocation, allocation);
        EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 0u);
        EXPECT_EQ(0u, device->usmReuseInfo.getAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(recycledAllocation);

        svmManager->trimUSMDeviceAllocCache();
        EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 0u);
        EXPECT_EQ(0u, device->usmReuseInfo.getAllocationsSavedForReuseSize());
    }
    {
        auto allocation = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation, nullptr);
        auto allocation2 = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation2, nullptr);
        EXPECT_EQ(0u, svmManager->usmDeviceAllocationsCache->allocations.size());
        EXPECT_EQ(0u, device->usmReuseInfo.getAllocationsSavedForReuseSize());

        svmManager->freeSVMAllocDefer(allocation);
        EXPECT_EQ(1u, svmManager->usmDeviceAllocationsCache->allocations.size());
        EXPECT_EQ(allocationSize, device->usmReuseInfo.getAllocationsSavedForReuseSize());

        svmManager->freeSVMAllocDefer(allocation2);
        EXPECT_EQ(1u, svmManager->usmDeviceAllocationsCache->allocations.size());
        EXPECT_EQ(allocationSize, device->usmReuseInfo.getAllocationsSavedForReuseSize());

        auto recycledAllocation = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(recycledAllocation, allocation);
        EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 0u);
        EXPECT_EQ(0u, device->usmReuseInfo.getAllocationsSavedForReuseSize());

        svmManager->freeSVMAllocDefer(recycledAllocation);

        svmManager->trimUSMDeviceAllocCache();
        EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 0u);
        EXPECT_EQ(0u, device->usmReuseInfo.getAllocationsSavedForReuseSize());
    }
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationCacheEnabledAndMultipleSVMManagersWhenFreeingDeviceAllocationThenItIsPutIntoCacheOnlyIfMaxSizeWillNotBeExceeded) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    auto secondSvmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    constexpr auto allocationSize = MemoryConstants::pageSize64k;
    device->usmReuseInfo.init(allocationSize, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    secondSvmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmDeviceAllocationsCache);
    ASSERT_NE(nullptr, secondSvmManager->usmDeviceAllocationsCache);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    {
        auto allocation = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation, nullptr);
        auto allocation2 = secondSvmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation2, nullptr);

        svmManager->freeSVMAlloc(allocation);
        EXPECT_EQ(1u, svmManager->usmDeviceAllocationsCache->allocations.size());
        EXPECT_EQ(allocationSize, device->usmReuseInfo.getAllocationsSavedForReuseSize());

        secondSvmManager->freeSVMAlloc(allocation2);
        EXPECT_EQ(0u, secondSvmManager->usmDeviceAllocationsCache->allocations.size());
        EXPECT_EQ(allocationSize, device->usmReuseInfo.getAllocationsSavedForReuseSize());

        auto recycledAllocation = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(recycledAllocation, allocation);
        EXPECT_EQ(0u, svmManager->usmDeviceAllocationsCache->allocations.size());
        EXPECT_EQ(0u, device->usmReuseInfo.getAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(recycledAllocation);

        svmManager->trimUSMDeviceAllocCache();
        EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 0u);
        EXPECT_EQ(0u, device->usmReuseInfo.getAllocationsSavedForReuseSize());
    }
    {
        auto allocation = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation, nullptr);
        auto allocation2 = secondSvmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation2, nullptr);

        secondSvmManager->freeSVMAlloc(allocation2);
        EXPECT_EQ(1u, secondSvmManager->usmDeviceAllocationsCache->allocations.size());
        EXPECT_EQ(allocationSize, device->usmReuseInfo.getAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(allocation);
        EXPECT_EQ(0u, svmManager->usmDeviceAllocationsCache->allocations.size());
        EXPECT_EQ(allocationSize, device->usmReuseInfo.getAllocationsSavedForReuseSize());

        auto recycledAllocation = secondSvmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(recycledAllocation, allocation2);
        EXPECT_EQ(0u, secondSvmManager->usmDeviceAllocationsCache->allocations.size());
        EXPECT_EQ(0u, device->usmReuseInfo.getAllocationsSavedForReuseSize());

        secondSvmManager->freeSVMAlloc(recycledAllocation);

        secondSvmManager->trimUSMDeviceAllocCache();
        EXPECT_EQ(secondSvmManager->usmDeviceAllocationsCache->allocations.size(), 0u);
        EXPECT_EQ(0u, device->usmReuseInfo.getAllocationsSavedForReuseSize());
    }
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationsWithDifferentSizesWhenAllocatingAfterFreeThenReturnCorrectCachedAllocation) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    device->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmDeviceAllocationsCache);

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
    ASSERT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), expectedCacheSize);

    for (auto const &testData : testDataset) {
        svmManager->freeSVMAlloc(testData.allocation);
    }

    ASSERT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), testDataset.size());

    std::vector<void *> allocationsToFree;

    for (auto &testData : testDataset) {
        auto secondAllocation = svmManager->createUnifiedMemoryAllocation(testData.allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), testDataset.size() - 1);
        EXPECT_EQ(secondAllocation, testData.allocation);
        svmManager->freeSVMAlloc(secondAllocation);
        EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), testDataset.size());
    }

    svmManager->cleanupUSMAllocCaches();
    EXPECT_EQ(nullptr, svmManager->usmDeviceAllocationsCache);
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationWithDifferentSizeWhenAllocatingAfterFreeThenCorrectSizeIsSet) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    device->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    EXPECT_NE(nullptr, svmManager->usmDeviceAllocationsCache);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;

    size_t firstAllocationSize = allocationSizeBasis - 2;
    size_t secondAllocationSize = allocationSizeBasis - 1;

    auto allocation = svmManager->createUnifiedMemoryAllocation(firstAllocationSize, unifiedMemoryProperties);
    EXPECT_NE(allocation, nullptr);

    size_t expectedCacheSize = 0u;
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), expectedCacheSize);

    svmManager->freeSVMAlloc(allocation);

    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 1u);

    SvmAllocationData *svmData = svmManager->getSVMAlloc(allocation);
    EXPECT_EQ(svmData->size, firstAllocationSize);

    auto secondAllocation = svmManager->createUnifiedMemoryAllocation(secondAllocationSize, unifiedMemoryProperties);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 0u);
    EXPECT_EQ(secondAllocation, allocation);

    svmManager->freeSVMAlloc(secondAllocation);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 1u);

    svmData = svmManager->getSVMAlloc(secondAllocation);
    EXPECT_EQ(svmData->size, secondAllocationSize);

    svmManager->cleanupUSMAllocCaches();
    EXPECT_EQ(nullptr, svmManager->usmDeviceAllocationsCache);
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationsWithDifferentSizesWhenAllocatingAfterFreeThenLimitMemoryWastage) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    device->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmDeviceAllocationsCache);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    auto allocation = svmManager->createUnifiedMemoryAllocation(SVMAllocsManager::SvmAllocationCache::minimalSizeToCheckUtilization, unifiedMemoryProperties);
    ASSERT_NE(allocation, nullptr);

    svmManager->freeSVMAlloc(allocation);

    ASSERT_EQ(1u, svmManager->usmDeviceAllocationsCache->allocations.size());

    constexpr auto allowedSizeForReuse = static_cast<size_t>(SVMAllocsManager::SvmAllocationCache::minimalSizeToCheckUtilization * SVMAllocsManager::SvmAllocationCache::minimalAllocUtilization);
    constexpr auto notAllowedSizeDueToMemoryWastage = allowedSizeForReuse - 1u;

    auto notReusedDueToMemoryWastage = svmManager->createUnifiedMemoryAllocation(notAllowedSizeDueToMemoryWastage, unifiedMemoryProperties);
    EXPECT_NE(nullptr, notReusedDueToMemoryWastage);
    EXPECT_NE(notReusedDueToMemoryWastage, allocation);
    EXPECT_EQ(1u, svmManager->usmDeviceAllocationsCache->allocations.size());

    auto reused = svmManager->createUnifiedMemoryAllocation(allowedSizeForReuse, unifiedMemoryProperties);
    EXPECT_NE(nullptr, notReusedDueToMemoryWastage);
    EXPECT_EQ(reused, allocation);
    EXPECT_EQ(0u, svmManager->usmDeviceAllocationsCache->allocations.size());

    svmManager->freeSVMAlloc(notReusedDueToMemoryWastage);
    svmManager->freeSVMAlloc(reused);
    svmManager->cleanupUSMAllocCaches();
    EXPECT_EQ(nullptr, svmManager->usmDeviceAllocationsCache);
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationOverSizeLimitWhenAllocatingAfterFreeThenDontSaveForReuse) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    device->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmDeviceAllocationsCache);
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

    EXPECT_EQ(0u, svmManager->usmDeviceAllocationsCache->allocations.size());
}

TEST_F(SvmDeviceAllocationCacheTest, givenMultipleAllocationsWhenAllocatingAfterFreeThenReturnAllocationsInCacheStartingFromSmallest) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    device->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmDeviceAllocationsCache);

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

    ASSERT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 0u);

    for (auto const &testData : testDataset) {
        svmManager->freeSVMAlloc(testData.allocation);
    }

    size_t expectedCacheSize = testDataset.size();
    ASSERT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), expectedCacheSize);

    auto allocationLargerThanInCache = svmManager->createUnifiedMemoryAllocation(allocationSizeBasis << 3, unifiedMemoryProperties);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), expectedCacheSize);

    auto firstAllocation = svmManager->createUnifiedMemoryAllocation(allocationSizeBasis, unifiedMemoryProperties);
    EXPECT_EQ(firstAllocation, testDataset[0].allocation);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), --expectedCacheSize);

    auto secondAllocation = svmManager->createUnifiedMemoryAllocation(allocationSizeBasis, unifiedMemoryProperties);
    EXPECT_EQ(secondAllocation, testDataset[1].allocation);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), --expectedCacheSize);

    auto thirdAllocation = svmManager->createUnifiedMemoryAllocation(allocationSizeBasis, unifiedMemoryProperties);
    EXPECT_EQ(thirdAllocation, testDataset[2].allocation);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 0u);

    svmManager->freeSVMAlloc(firstAllocation);
    svmManager->freeSVMAlloc(secondAllocation);
    svmManager->freeSVMAlloc(thirdAllocation);
    svmManager->freeSVMAlloc(allocationLargerThanInCache);

    svmManager->cleanupUSMAllocCaches();
    EXPECT_EQ(nullptr, svmManager->usmDeviceAllocationsCache);
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
    auto deviceFactory = std::make_unique<UltDeviceFactory>(2, 2);
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto rootDevice = deviceFactory->rootDevices[0];
    auto secondRootDevice = deviceFactory->rootDevices[1];
    auto subDevice1 = reinterpret_cast<MockSubDevice *>(deviceFactory->subDevices[0]);
    auto svmManager = std::make_unique<MockSVMAllocsManager>(rootDevice->getMemoryManager());
    rootDevice->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    secondRootDevice->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    subDevice1->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*rootDevice);
    ASSERT_NE(nullptr, svmManager->usmDeviceAllocationsCache);

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
        ASSERT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 0u);

        for (auto &testData : testDataset) {
            svmManager->freeSVMAlloc(testData.allocation);
        }
        ASSERT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), testDataset.size());

        auto allocationFromCache = svmManager->createUnifiedMemoryAllocation(allocationDataToVerify.allocationSize, allocationDataToVerify.unifiedMemoryProperties);
        EXPECT_EQ(allocationFromCache, allocationDataToVerify.allocation);

        auto allocationNotFromCache = svmManager->createUnifiedMemoryAllocation(allocationDataToVerify.allocationSize, allocationDataToVerify.unifiedMemoryProperties);
        for (auto &cachedAllocation : testDataset) {
            EXPECT_NE(allocationNotFromCache, cachedAllocation.allocation);
        }
        svmManager->freeSVMAlloc(allocationFromCache);
        svmManager->freeSVMAlloc(allocationNotFromCache);

        svmManager->trimUSMDeviceAllocCache();
        ASSERT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 0u);
    }
}

TEST_F(SvmDeviceAllocationCacheTest, givenDeviceOutOfMemoryWhenAllocatingThenCacheIsTrimmedAndAllocationSucceeds) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    device->injectMemoryManager(new MockMemoryManagerWithCapacity(*device->getExecutionEnvironment()));
    MockMemoryManagerWithCapacity *memoryManager = static_cast<MockMemoryManagerWithCapacity *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    device->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    memoryManager->usmReuseInfo.init(0u, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmDeviceAllocationsCache);

    memoryManager->capacity = MemoryConstants::pageSize64k * 3;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;

    auto allocationInCache = svmManager->createUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties);
    auto allocationInCache2 = svmManager->createUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties);
    auto allocationInCache3 = svmManager->createUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties);
    ASSERT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 0u);
    svmManager->freeSVMAlloc(allocationInCache);
    svmManager->freeSVMAlloc(allocationInCache2);
    svmManager->freeSVMAllocDefer(allocationInCache3);

    ASSERT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 3u);
    ASSERT_NE(svmManager->getSVMAlloc(allocationInCache), nullptr);
    ASSERT_NE(svmManager->getSVMAlloc(allocationInCache2), nullptr);
    ASSERT_NE(svmManager->getSVMAlloc(allocationInCache3), nullptr);
    auto ptr = svmManager->createUnifiedMemoryAllocation(MemoryConstants::pageSize64k * 2, unifiedMemoryProperties);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 0u);
    svmManager->freeSVMAlloc(ptr);

    svmManager->cleanupUSMAllocCaches();
    EXPECT_EQ(nullptr, svmManager->usmDeviceAllocationsCache);
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationWithIsInternalAllocationSetWhenAllocatingAfterFreeThenDoNotReuseAllocation) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    device->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    EXPECT_NE(nullptr, svmManager->usmDeviceAllocationsCache);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    auto allocation = svmManager->createUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    EXPECT_NE(allocation, nullptr);
    svmManager->freeSVMAlloc(allocation);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 1u);

    unifiedMemoryProperties.isInternalAllocation = true;
    auto testedAllocation = svmManager->createUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 1u);
    auto svmData = svmManager->getSVMAlloc(testedAllocation);
    EXPECT_NE(nullptr, svmData);
    EXPECT_TRUE(svmData->isInternalAllocation);

    svmManager->freeSVMAlloc(testedAllocation);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 1u);

    svmManager->cleanupUSMAllocCaches();
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationInUsageWhenAllocatingAfterFreeThenDoNotReuseAllocation) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    device->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    EXPECT_NE(nullptr, svmManager->usmDeviceAllocationsCache);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    auto allocation = svmManager->createUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    EXPECT_NE(allocation, nullptr);
    svmManager->freeSVMAlloc(allocation);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 1u);

    MockMemoryManager *mockMemoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    mockMemoryManager->deferAllocInUse = true;
    auto testedAllocation = svmManager->createUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 1u);
    auto svmData = svmManager->getSVMAlloc(testedAllocation);
    EXPECT_NE(nullptr, svmData);

    svmManager->freeSVMAlloc(testedAllocation);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 2u);

    svmManager->cleanupUSMAllocCaches();
}

TEST_F(SvmDeviceAllocationCacheTest, givenUsmReuseCleanerWhenTrimOldInCachesCalledThenOldAllocationsAreRemovedIfDeferredDeleterHasNoWork) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(0);
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    memoryManager->usmReuseInfo.init(0u, UsmReuseInfo::notLimited);
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    device->executionEnvironment->unifiedMemoryReuseCleaner.reset(new MockUnifiedMemoryReuseCleaner(false));
    auto mockUnifiedMemoryReuseCleaner = reinterpret_cast<MockUnifiedMemoryReuseCleaner *>(device->executionEnvironment->unifiedMemoryReuseCleaner.get());
    EXPECT_EQ(0u, mockUnifiedMemoryReuseCleaner->svmAllocationCaches.size());
    device->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    EXPECT_NE(nullptr, svmManager->usmDeviceAllocationsCache);
    EXPECT_EQ(1u, mockUnifiedMemoryReuseCleaner->svmAllocationCaches.size());
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.get(), mockUnifiedMemoryReuseCleaner->svmAllocationCaches[0]);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    auto allocation = svmManager->createUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    auto allocation2 = svmManager->createUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    EXPECT_NE(allocation, nullptr);
    EXPECT_NE(allocation2, nullptr);
    svmManager->freeSVMAlloc(allocation);
    svmManager->freeSVMAlloc(allocation2);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 2u);

    const auto baseTimePoint = std::chrono::high_resolution_clock::now();
    const auto oldTimePoint = baseTimePoint - UnifiedMemoryReuseCleaner::maxHoldTime;
    const auto notTrimmedTimePoint = baseTimePoint + std::chrono::hours(24);

    svmManager->usmDeviceAllocationsCache->allocations[0].saveTime = oldTimePoint;
    svmManager->usmDeviceAllocationsCache->allocations[1].saveTime = notTrimmedTimePoint;

    memoryManager->setDeferredDeleter(new MockDeferredDeleter);
    mockUnifiedMemoryReuseCleaner->trimOldInCaches();
    EXPECT_EQ(2u, svmManager->usmDeviceAllocationsCache->allocations.size());

    mockUnifiedMemoryReuseCleaner->trimOldInCaches();
    EXPECT_EQ(1u, svmManager->usmDeviceAllocationsCache->allocations.size());
    EXPECT_EQ(notTrimmedTimePoint, svmManager->usmDeviceAllocationsCache->allocations[0].saveTime);

    svmManager->usmDeviceAllocationsCache->allocations[0].saveTime = oldTimePoint;
    memoryManager->setDeferredDeleter(nullptr);
    mockUnifiedMemoryReuseCleaner->trimOldInCaches();
    EXPECT_EQ(0u, svmManager->usmDeviceAllocationsCache->allocations.size());

    svmManager->cleanupUSMAllocCaches();
    EXPECT_EQ(0u, mockUnifiedMemoryReuseCleaner->svmAllocationCaches.size());
}

TEST_F(SvmDeviceAllocationCacheTest, givenDirectSubmissionLightWhenTrimOldInCachesCalledThenAllOldAllocationsAreRemoved) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(0);
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    memoryManager->usmReuseInfo.init(0u, UsmReuseInfo::notLimited);
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    device->executionEnvironment->unifiedMemoryReuseCleaner.reset(new MockUnifiedMemoryReuseCleaner(true));
    auto mockUnifiedMemoryReuseCleaner = reinterpret_cast<MockUnifiedMemoryReuseCleaner *>(device->executionEnvironment->unifiedMemoryReuseCleaner.get());
    EXPECT_EQ(0u, mockUnifiedMemoryReuseCleaner->svmAllocationCaches.size());
    device->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    EXPECT_NE(nullptr, svmManager->usmDeviceAllocationsCache);
    EXPECT_EQ(1u, mockUnifiedMemoryReuseCleaner->svmAllocationCaches.size());
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.get(), mockUnifiedMemoryReuseCleaner->svmAllocationCaches[0]);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    auto allocation = svmManager->createUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    auto allocation2 = svmManager->createUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    EXPECT_NE(allocation, nullptr);
    EXPECT_NE(allocation2, nullptr);
    svmManager->freeSVMAlloc(allocation);
    svmManager->freeSVMAlloc(allocation2);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 2u);

    const auto baseTimePoint = std::chrono::high_resolution_clock::now();
    const auto oldTimePoint = baseTimePoint - UnifiedMemoryReuseCleaner::maxHoldTime;

    svmManager->usmDeviceAllocationsCache->allocations[0].saveTime = oldTimePoint;
    svmManager->usmDeviceAllocationsCache->allocations[1].saveTime = oldTimePoint;

    mockUnifiedMemoryReuseCleaner->trimOldInCaches();
    EXPECT_EQ(0u, svmManager->usmDeviceAllocationsCache->allocations.size());

    svmManager->cleanupUSMAllocCaches();
    EXPECT_EQ(0u, mockUnifiedMemoryReuseCleaner->svmAllocationCaches.size());
}

TEST_F(SvmDeviceAllocationCacheTest, givenUsmReuseCleanerWhenTrimOldInCachesCalledAndShouldLimitUsmReuseThenAllOldAllocationsAreRemovedEvenIfDeferredDeleterHasWork) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(0);
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    memoryManager->usmReuseInfo.init(0u, UsmReuseInfo::notLimited);
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    device->executionEnvironment->unifiedMemoryReuseCleaner.reset(new MockUnifiedMemoryReuseCleaner(false));
    auto mockUnifiedMemoryReuseCleaner = reinterpret_cast<MockUnifiedMemoryReuseCleaner *>(device->executionEnvironment->unifiedMemoryReuseCleaner.get());
    EXPECT_EQ(0u, mockUnifiedMemoryReuseCleaner->svmAllocationCaches.size());
    device->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    EXPECT_NE(nullptr, svmManager->usmDeviceAllocationsCache);
    EXPECT_EQ(1u, mockUnifiedMemoryReuseCleaner->svmAllocationCaches.size());
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache.get(), mockUnifiedMemoryReuseCleaner->svmAllocationCaches[0]);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    auto allocation = svmManager->createUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    auto allocation2 = svmManager->createUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    EXPECT_NE(allocation, nullptr);
    EXPECT_NE(allocation2, nullptr);
    svmManager->freeSVMAlloc(allocation);
    svmManager->freeSVMAlloc(allocation2);
    EXPECT_EQ(svmManager->usmDeviceAllocationsCache->allocations.size(), 2u);

    const auto baseTimePoint = std::chrono::high_resolution_clock::now();
    const auto oldTimePoint = baseTimePoint - UnifiedMemoryReuseCleaner::limitedHoldTime;

    svmManager->usmDeviceAllocationsCache->allocations[0].saveTime = oldTimePoint;
    svmManager->usmDeviceAllocationsCache->allocations[1].saveTime = oldTimePoint;

    memoryManager->setDeferredDeleter(new MockDeferredDeleter);
    memoryManager->usmReuseInfo.init(1 * MemoryConstants::gigaByte, alwaysLimited);
    mockUnifiedMemoryReuseCleaner->trimOldInCaches();
    EXPECT_EQ(0u, svmManager->usmDeviceAllocationsCache->allocations.size());

    svmManager->cleanupUSMAllocCaches();
}

TEST_F(SvmDeviceAllocationCacheTest, givenAllocationsInReuseWhenTrimOldAllocsCalledThenTrimAllocationsSavedBeforeTimePointLargestFirst) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableDeviceAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    device->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    EXPECT_NE(nullptr, svmManager->usmDeviceAllocationsCache);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;
    auto allocation = svmManager->createUnifiedMemoryAllocation(1 * MemoryConstants::pageSize64k, unifiedMemoryProperties);
    auto allocation2 = svmManager->createUnifiedMemoryAllocation(2 * MemoryConstants::pageSize64k, unifiedMemoryProperties);
    auto allocation3 = svmManager->createUnifiedMemoryAllocation(3 * MemoryConstants::pageSize64k, unifiedMemoryProperties);
    EXPECT_NE(allocation, nullptr);
    EXPECT_NE(allocation2, nullptr);
    EXPECT_NE(allocation3, nullptr);
    svmManager->freeSVMAlloc(allocation);
    svmManager->freeSVMAlloc(allocation2);
    svmManager->freeSVMAlloc(allocation3);
    EXPECT_EQ(3u, svmManager->usmDeviceAllocationsCache->allocations.size());
    EXPECT_EQ(1 * MemoryConstants::pageSize64k, svmManager->usmDeviceAllocationsCache->allocations[0].allocationSize);
    EXPECT_EQ(2 * MemoryConstants::pageSize64k, svmManager->usmDeviceAllocationsCache->allocations[1].allocationSize);
    EXPECT_EQ(3 * MemoryConstants::pageSize64k, svmManager->usmDeviceAllocationsCache->allocations[2].allocationSize);

    const auto baseTimePoint = std::chrono::high_resolution_clock::now();
    const auto timeDiff = std::chrono::microseconds(1);

    svmManager->usmDeviceAllocationsCache->allocations[0].saveTime = baseTimePoint;
    svmManager->usmDeviceAllocationsCache->allocations[1].saveTime = baseTimePoint + timeDiff * 2;
    svmManager->usmDeviceAllocationsCache->allocations[2].saveTime = baseTimePoint + timeDiff;

    svmManager->usmDeviceAllocationsCache->trimOldAllocs(baseTimePoint + timeDiff, false);
    EXPECT_EQ(2u, svmManager->usmDeviceAllocationsCache->allocations.size());
    EXPECT_EQ(1 * MemoryConstants::pageSize64k, svmManager->usmDeviceAllocationsCache->allocations[0].allocationSize);
    EXPECT_EQ(2 * MemoryConstants::pageSize64k, svmManager->usmDeviceAllocationsCache->allocations[1].allocationSize);

    svmManager->usmDeviceAllocationsCache->trimOldAllocs(baseTimePoint + timeDiff, false);
    EXPECT_EQ(1u, svmManager->usmDeviceAllocationsCache->allocations.size());
    EXPECT_EQ(2 * MemoryConstants::pageSize64k, svmManager->usmDeviceAllocationsCache->allocations[0].allocationSize);

    svmManager->usmDeviceAllocationsCache->trimOldAllocs(baseTimePoint + timeDiff, false);
    EXPECT_EQ(1u, svmManager->usmDeviceAllocationsCache->allocations.size());
    EXPECT_EQ(baseTimePoint + timeDiff * 2, svmManager->usmDeviceAllocationsCache->allocations[0].saveTime);

    svmManager->cleanupUSMAllocCaches();
}

using SvmHostAllocationCacheTest = Test<SvmAllocationCacheTestFixture>;

TEST_F(SvmHostAllocationCacheTest, givenAllocationCacheDisabledWhenCheckingIfEnabledThenItIsDisabled) {
    DebugManagerStateRestore restorer;
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(0);
    EXPECT_EQ(nullptr, svmManager->usmHostAllocationsCache);
    svmManager->initUsmAllocationsCaches(*device);
    EXPECT_EQ(nullptr, svmManager->usmHostAllocationsCache);

    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto allocation = svmManager->createHostUnifiedMemoryAllocation(1u, unifiedMemoryProperties);
    EXPECT_NE(allocation, nullptr);
    svmManager->freeSVMAlloc(allocation);
    EXPECT_EQ(nullptr, svmManager->usmHostAllocationsCache);

    allocation = svmManager->createHostUnifiedMemoryAllocation(1u, unifiedMemoryProperties);
    EXPECT_NE(allocation, nullptr);
    svmManager->freeSVMAllocDefer(allocation);
    EXPECT_EQ(nullptr, svmManager->usmHostAllocationsCache);
}

HWTEST_F(SvmHostAllocationCacheTest, givenOclApiSpecificConfigAndProductHelperAndDebuggerWhenCheckingIfEnabledThenEnableCorrectly) {
    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    MockDevice *device = deviceFactory->rootDevices[0];
    device->initUsmReuseLimits();
    RAIIProductHelperFactory<MockProductHelper> raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    const auto expectedMaxSize = static_cast<size_t>(0.02 * device->getMemoryManager()->getSystemSharedMemory(0u));
    EXPECT_EQ(expectedMaxSize, device->getMemoryManager()->usmReuseInfo.getMaxAllocationsSavedForReuseSize());

    {
        raii.mockProductHelper->isHostUsmAllocationReuseSupportedResult = false;
        device->getMemoryManager()->initUsmReuseLimits();
        auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
        EXPECT_EQ(nullptr, svmManager->usmHostAllocationsCache);
        svmManager->initUsmAllocationsCaches(*device);
        EXPECT_EQ(nullptr, svmManager->usmHostAllocationsCache);
    }
    {
        raii.mockProductHelper->isHostUsmAllocationReuseSupportedResult = true;
        device->getMemoryManager()->initUsmReuseLimits();
        auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
        EXPECT_EQ(nullptr, svmManager->usmHostAllocationsCache);
        svmManager->initUsmAllocationsCaches(*device);
        EXPECT_NE(nullptr, svmManager->usmHostAllocationsCache);
    }
    {
        device->getRootDeviceEnvironmentRef().debugger.reset(new MockDebugger);
        raii.mockProductHelper->isHostUsmAllocationReuseSupportedResult = true;
        device->getMemoryManager()->initUsmReuseLimits();
        auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
        EXPECT_EQ(nullptr, svmManager->usmHostAllocationsCache);
        svmManager->initUsmAllocationsCaches(*device);
        EXPECT_EQ(nullptr, svmManager->usmHostAllocationsCache);

        device->getRootDeviceEnvironmentRef().debugger.reset(nullptr);
        svmManager->initUsmAllocationsCaches(*device);
        EXPECT_NE(nullptr, svmManager->usmHostAllocationsCache);
    }

    for (int32_t csrType = 0;
         csrType < static_cast<int32_t>(CommandStreamReceiverType::typesNum);
         ++csrType) {
        DebugManagerStateRestore restorer;
        debugManager.flags.SetCommandStreamReceiver.set(csrType);
        debugManager.flags.ExperimentalEnableHostAllocationCache.set(2);
        raii.mockProductHelper->isHostUsmAllocationReuseSupportedResult = true;
        device->getMemoryManager()->initUsmReuseLimits();
        auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
        EXPECT_EQ(nullptr, svmManager->usmHostAllocationsCache);
        svmManager->initUsmAllocationsCaches(*device);
        ASSERT_NE(nullptr, svmManager->usmHostAllocationsCache);
        EXPECT_EQ(csrType != 0, svmManager->usmHostAllocationsCache->requireUpdatingAllocsForIndirectAccess);
    }

    for (int32_t csrType = 0;
         csrType < static_cast<int32_t>(CommandStreamReceiverType::typesNum);
         ++csrType) {
        DebugManagerStateRestore restorer;
        debugManager.flags.SetCommandStreamReceiver.set(csrType);
        raii.mockProductHelper->isHostUsmAllocationReuseSupportedResult = true;
        device->getMemoryManager()->initUsmReuseLimits();
        auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
        EXPECT_EQ(nullptr, svmManager->usmHostAllocationsCache);
        svmManager->initUsmAllocationsCaches(*device);
        EXPECT_EQ(NEO::DeviceFactory::isHwModeSelected(), nullptr != svmManager->usmHostAllocationsCache);
    }
}

TEST_F(SvmHostAllocationCacheTest, givenReuseLimitFlagWhenInitUsmReuseLimitCalledThenLimitThresholdSetCorrectly) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());

    {
        DebugManagerStateRestore restore;
        debugManager.flags.ExperimentalUSMAllocationReuseLimitThreshold.set(0);
        memoryManager->initUsmReuseLimits();
        EXPECT_EQ(UsmReuseInfo::notLimited, memoryManager->usmReuseInfo.getLimitAllocationsReuseThreshold());
    }
    {
        DebugManagerStateRestore restore;
        debugManager.flags.ExperimentalUSMAllocationReuseLimitThreshold.set(70);
        memoryManager->initUsmReuseLimits();
        const auto systemSharedMemory = memoryManager->getSystemSharedMemory(device->getRootDeviceIndex());
        const auto expectedLimitThreshold = static_cast<uint64_t>(0.7 * systemSharedMemory);
        EXPECT_EQ(expectedLimitThreshold, memoryManager->usmReuseInfo.getLimitAllocationsReuseThreshold());
    }
    {
        DebugManagerStateRestore restore;
        debugManager.flags.ExperimentalUSMAllocationReuseLimitThreshold.set(-1);
        memoryManager->initUsmReuseLimits();
        const auto systemSharedMemory = memoryManager->getSystemSharedMemory(device->getRootDeviceIndex());
        const auto expectedLimitThreshold = static_cast<uint64_t>(0.8 * systemSharedMemory);
        EXPECT_EQ(expectedLimitThreshold, memoryManager->usmReuseInfo.getLimitAllocationsReuseThreshold());
    }
}

struct SvmHostAllocationCacheSimpleTestDataType {
    size_t allocationSize;
    void *allocation;
};

TEST_F(SvmHostAllocationCacheTest, givenAllocationCacheEnabledWhenFreeingHostAllocationThenItIsPutIntoCache) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    memoryManager->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmHostAllocationsCache);

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
    ASSERT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), expectedCacheSize);

    for (auto const &testData : testDataset) {
        svmManager->freeSVMAlloc(testData.allocation);
        EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), ++expectedCacheSize);
        bool foundInCache = false;
        for (auto i = 0u; i < svmManager->usmHostAllocationsCache->allocations.size(); ++i) {
            if (svmManager->usmHostAllocationsCache->allocations[i].allocation == testData.allocation) {
                foundInCache = true;
                auto svmData = svmManager->getSVMAlloc(testData.allocation);
                EXPECT_NE(nullptr, svmData);
                EXPECT_EQ(svmData, svmManager->usmHostAllocationsCache->allocations[i].svmData);
                EXPECT_EQ(svmData->gpuAllocations.getDefaultGraphicsAllocation()->getUnderlyingBufferSize(),
                          svmManager->usmHostAllocationsCache->allocations[i].allocationSize);
                break;
            }
        }
        EXPECT_TRUE(foundInCache);
    }
    EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), testDataset.size());

    svmManager->cleanupUSMAllocCaches();
    EXPECT_EQ(nullptr, svmManager->usmHostAllocationsCache);
}

TEST_F(SvmHostAllocationCacheTest, givenAllocationCacheEnabledWhenInitializedThenMaxSizeIsSetCorrectly) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(2);
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmHostAllocationsCache);

    auto expectedMaxSize = static_cast<size_t>(svmManager->memoryManager->getSystemSharedMemory(mockRootDeviceIndex) * 0.02);
    EXPECT_EQ(expectedMaxSize, device->getMemoryManager()->usmReuseInfo.getMaxAllocationsSavedForReuseSize());
}

TEST_F(SvmHostAllocationCacheTest, givenAllocationCacheEnabledWhenFreeingHostAllocationThenItIsPutIntoCacheOnlyIfMaxSizeWillNotBeExceeded) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    constexpr auto allocationSize = MemoryConstants::pageSize64k;
    memoryManager->usmReuseInfo.init(allocationSize, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmHostAllocationsCache);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    {
        auto allocation = svmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation, nullptr);
        auto allocation2 = svmManager->createHostUnifiedMemoryAllocation(1u, unifiedMemoryProperties);
        ASSERT_NE(allocation2, nullptr);
        EXPECT_EQ(0u, svmManager->usmHostAllocationsCache->allocations.size());
        EXPECT_EQ(0u, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(allocation);
        EXPECT_EQ(1u, svmManager->usmHostAllocationsCache->allocations.size());
        EXPECT_EQ(allocationSize, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(allocation2);
        EXPECT_EQ(1u, svmManager->usmHostAllocationsCache->allocations.size());
        EXPECT_EQ(allocationSize, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

        auto recycledAllocation = svmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(recycledAllocation, allocation);
        EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 0u);
        EXPECT_EQ(0u, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(recycledAllocation);

        svmManager->trimUSMHostAllocCache();
        EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 0u);
        EXPECT_EQ(0u, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());
    }
    {
        auto allocation = svmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation, nullptr);
        auto allocation2 = svmManager->createHostUnifiedMemoryAllocation(1u, unifiedMemoryProperties);
        ASSERT_NE(allocation2, nullptr);
        EXPECT_EQ(0u, svmManager->usmHostAllocationsCache->allocations.size());
        EXPECT_EQ(0u, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

        svmManager->freeSVMAllocDefer(allocation);
        EXPECT_EQ(1u, svmManager->usmHostAllocationsCache->allocations.size());
        EXPECT_EQ(allocationSize, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

        svmManager->freeSVMAllocDefer(allocation2);
        EXPECT_EQ(1u, svmManager->usmHostAllocationsCache->allocations.size());
        EXPECT_EQ(allocationSize, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

        auto recycledAllocation = svmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(recycledAllocation, allocation);
        EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 0u);
        EXPECT_EQ(0u, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

        svmManager->freeSVMAllocDefer(recycledAllocation);

        svmManager->trimUSMHostAllocCache();
        EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 0u);
        EXPECT_EQ(0u, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());
    }
}

TEST_F(SvmHostAllocationCacheTest, givenAllocationCacheEnabledWhenFreeingHostAllocationAndShouldLimitUsmReuseThenItIsNotPutIntoCache) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    constexpr auto allocationSize = MemoryConstants::pageSize64k;
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmHostAllocationsCache);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto allocation = svmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
    ASSERT_NE(allocation, nullptr);
    auto allocation2 = svmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
    ASSERT_NE(allocation2, nullptr);
    EXPECT_EQ(0u, svmManager->usmHostAllocationsCache->allocations.size());
    EXPECT_EQ(0u, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

    svmManager->freeSVMAlloc(allocation);
    EXPECT_EQ(1u, svmManager->usmHostAllocationsCache->allocations.size());
    EXPECT_EQ(allocationSize, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

    memoryManager->usmReuseInfo.init(1 * MemoryConstants::gigaByte, alwaysLimited);
    svmManager->freeSVMAlloc(allocation2);
    EXPECT_EQ(1u, svmManager->usmHostAllocationsCache->allocations.size());
    EXPECT_EQ(allocationSize, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

    svmManager->cleanupUSMAllocCaches();
}

TEST_F(SvmHostAllocationCacheTest, givenAllocationCacheEnabledAndMultipleSVMManagersWhenFreeingHostAllocationThenItIsPutIntoCacheOnlyIfMaxSizeWillNotBeExceeded) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    auto secondSvmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    constexpr auto allocationSize = MemoryConstants::pageSize64k;
    memoryManager->usmReuseInfo.init(allocationSize, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    secondSvmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmHostAllocationsCache);
    ASSERT_NE(nullptr, secondSvmManager->usmHostAllocationsCache);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    {
        auto allocation = svmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation, nullptr);
        auto allocation2 = secondSvmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation2, nullptr);
        EXPECT_EQ(0u, svmManager->usmHostAllocationsCache->allocations.size());
        EXPECT_EQ(0u, secondSvmManager->usmHostAllocationsCache->allocations.size());
        EXPECT_EQ(0u, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(allocation);
        EXPECT_EQ(1u, svmManager->usmHostAllocationsCache->allocations.size());
        EXPECT_EQ(allocationSize, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

        secondSvmManager->freeSVMAlloc(allocation2);
        EXPECT_EQ(0u, secondSvmManager->usmHostAllocationsCache->allocations.size());
        EXPECT_EQ(allocationSize, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

        auto recycledAllocation = svmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(recycledAllocation, allocation);
        EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 0u);
        EXPECT_EQ(0u, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(recycledAllocation);

        svmManager->trimUSMHostAllocCache();
        EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 0u);
        EXPECT_EQ(0u, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());
    }
    {
        auto allocation = svmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation, nullptr);
        auto allocation2 = secondSvmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        ASSERT_NE(allocation2, nullptr);
        EXPECT_EQ(0u, svmManager->usmHostAllocationsCache->allocations.size());
        EXPECT_EQ(0u, secondSvmManager->usmHostAllocationsCache->allocations.size());
        EXPECT_EQ(0u, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

        secondSvmManager->freeSVMAlloc(allocation2);
        EXPECT_EQ(1u, secondSvmManager->usmHostAllocationsCache->allocations.size());
        EXPECT_EQ(allocationSize, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

        svmManager->freeSVMAlloc(allocation);
        EXPECT_EQ(0u, svmManager->usmHostAllocationsCache->allocations.size());
        EXPECT_EQ(allocationSize, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

        auto recycledAllocation = secondSvmManager->createHostUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(recycledAllocation, allocation2);
        EXPECT_EQ(secondSvmManager->usmHostAllocationsCache->allocations.size(), 0u);
        EXPECT_EQ(0u, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());

        secondSvmManager->freeSVMAlloc(recycledAllocation);

        secondSvmManager->trimUSMHostAllocCache();
        EXPECT_EQ(secondSvmManager->usmHostAllocationsCache->allocations.size(), 0u);
        EXPECT_EQ(0u, memoryManager->usmReuseInfo.getAllocationsSavedForReuseSize());
    }
}

TEST_F(SvmHostAllocationCacheTest, givenAllocationsWithDifferentSizesWhenAllocatingAfterFreeThenReturnCorrectCachedAllocationAndSetAubTbxWritable) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    memoryManager->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmHostAllocationsCache);

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
    ASSERT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), expectedCacheSize);

    for (auto const &testData : testDataset) {
        svmManager->freeSVMAlloc(testData.allocation);
    }

    ASSERT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), testDataset.size());

    std::vector<void *> allocationsToFree;

    constexpr auto allBanks = std::numeric_limits<uint32_t>::max();
    for (auto allocInfo : svmManager->usmHostAllocationsCache->allocations) {
        allocInfo.svmData->gpuAllocations.getDefaultGraphicsAllocation()->setAubWritable(false, allBanks);
        allocInfo.svmData->gpuAllocations.getDefaultGraphicsAllocation()->setTbxWritable(false, allBanks);
    }

    for (auto &testData : testDataset) {
        auto secondAllocation = svmManager->createHostUnifiedMemoryAllocation(testData.allocationSize, unifiedMemoryProperties);
        EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), testDataset.size() - 1);
        EXPECT_EQ(secondAllocation, testData.allocation);
        auto allocData = svmManager->getSVMAlloc(secondAllocation);
        EXPECT_NE(nullptr, allocData);
        EXPECT_TRUE(allocData->gpuAllocations.getDefaultGraphicsAllocation()->isAubWritable(allBanks));
        EXPECT_TRUE(allocData->gpuAllocations.getDefaultGraphicsAllocation()->isTbxWritable(allBanks));
        svmManager->freeSVMAlloc(secondAllocation);
        EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), testDataset.size());
    }

    svmManager->cleanupUSMAllocCaches();
    EXPECT_EQ(nullptr, svmManager->usmHostAllocationsCache);
}

TEST_F(SvmHostAllocationCacheTest, givenAllocationsWithDifferentSizesWhenAllocatingAfterFreeThenLimitMemoryWastage) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    memoryManager->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmHostAllocationsCache);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto allocation = svmManager->createHostUnifiedMemoryAllocation(SVMAllocsManager::SvmAllocationCache::minimalSizeToCheckUtilization, unifiedMemoryProperties);
    ASSERT_NE(allocation, nullptr);

    svmManager->freeSVMAlloc(allocation);

    ASSERT_EQ(1u, svmManager->usmHostAllocationsCache->allocations.size());

    constexpr auto allowedSizeForReuse = static_cast<size_t>(SVMAllocsManager::SvmAllocationCache::minimalSizeToCheckUtilization * SVMAllocsManager::SvmAllocationCache::minimalAllocUtilization);
    constexpr auto notAllowedSizeDueToMemoryWastage = allowedSizeForReuse - 1u;

    auto notReusedDueToMemoryWastage = svmManager->createHostUnifiedMemoryAllocation(notAllowedSizeDueToMemoryWastage, unifiedMemoryProperties);
    EXPECT_NE(nullptr, notReusedDueToMemoryWastage);
    EXPECT_NE(notReusedDueToMemoryWastage, allocation);
    EXPECT_EQ(1u, svmManager->usmHostAllocationsCache->allocations.size());

    auto reused = svmManager->createHostUnifiedMemoryAllocation(allowedSizeForReuse, unifiedMemoryProperties);
    EXPECT_NE(nullptr, notReusedDueToMemoryWastage);
    EXPECT_EQ(reused, allocation);
    EXPECT_EQ(0u, svmManager->usmHostAllocationsCache->allocations.size());

    svmManager->freeSVMAlloc(notReusedDueToMemoryWastage);
    svmManager->freeSVMAlloc(reused);
    svmManager->cleanupUSMAllocCaches();
    EXPECT_EQ(nullptr, svmManager->usmHostAllocationsCache);
}

TEST_F(SvmHostAllocationCacheTest, givenAllocationOverSizeLimitWhenAllocatingAfterFreeThenDontSaveForReuse) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    memoryManager->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmHostAllocationsCache);
    const auto notAcceptedAllocSize = SVMAllocsManager::SvmAllocationCache::maxServicedSize + 1;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto mockGa = std::make_unique<MockGraphicsAllocation>(mockRootDeviceIndex, nullptr, notAcceptedAllocSize);
    mockGa->gpuAddress = 0xbadf00;
    mockGa->cpuPtr = reinterpret_cast<void *>(0xbadf00);
    mockGa->setAllocationType(AllocationType::svmCpu);
    memoryManager->mockGa = mockGa.release();
    memoryManager->returnMockGAFromHostPool = true;
    auto allocation = svmManager->createHostUnifiedMemoryAllocation(notAcceptedAllocSize, unifiedMemoryProperties);
    EXPECT_EQ(reinterpret_cast<void *>(0xbadf00), allocation);

    svmManager->freeSVMAlloc(allocation);

    EXPECT_EQ(0u, svmManager->usmHostAllocationsCache->allocations.size());
}

TEST_F(SvmHostAllocationCacheTest, givenMultipleAllocationsWhenAllocatingAfterFreeThenReturnAllocationsInCacheStartingFromSmallest) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    memoryManager->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmHostAllocationsCache);

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

    ASSERT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 0u);

    for (auto const &testData : testDataset) {
        svmManager->freeSVMAlloc(testData.allocation);
    }

    size_t expectedCacheSize = testDataset.size();
    ASSERT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), expectedCacheSize);

    auto allocationLargerThanInCache = svmManager->createHostUnifiedMemoryAllocation(allocationSizeBasis << 3, unifiedMemoryProperties);
    EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), expectedCacheSize);

    auto firstAllocation = svmManager->createHostUnifiedMemoryAllocation(allocationSizeBasis, unifiedMemoryProperties);
    EXPECT_EQ(firstAllocation, testDataset[0].allocation);
    EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), --expectedCacheSize);

    auto secondAllocation = svmManager->createHostUnifiedMemoryAllocation(allocationSizeBasis, unifiedMemoryProperties);
    EXPECT_EQ(secondAllocation, testDataset[1].allocation);
    EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), --expectedCacheSize);

    auto thirdAllocation = svmManager->createHostUnifiedMemoryAllocation(allocationSizeBasis, unifiedMemoryProperties);
    EXPECT_EQ(thirdAllocation, testDataset[2].allocation);
    EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 0u);

    svmManager->freeSVMAlloc(firstAllocation);
    svmManager->freeSVMAlloc(secondAllocation);
    svmManager->freeSVMAlloc(thirdAllocation);
    svmManager->freeSVMAlloc(allocationLargerThanInCache);

    svmManager->cleanupUSMAllocCaches();
    EXPECT_EQ(nullptr, svmManager->usmHostAllocationsCache);
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
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto rootDevice = deviceFactory->rootDevices[0];
    auto memoryManager = reinterpret_cast<MockMemoryManager *>(rootDevice->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    memoryManager->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*rootDevice);
    ASSERT_NE(nullptr, svmManager->usmHostAllocationsCache);

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
        ASSERT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 0u);

        for (auto &testData : testDataset) {
            svmManager->freeSVMAlloc(testData.allocation);
        }
        ASSERT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), testDataset.size());

        auto allocationFromCache = svmManager->createHostUnifiedMemoryAllocation(allocationDataToVerify.allocationSize, allocationDataToVerify.unifiedMemoryProperties);
        EXPECT_EQ(allocationFromCache, allocationDataToVerify.allocation);

        auto allocationNotFromCache = svmManager->createHostUnifiedMemoryAllocation(allocationDataToVerify.allocationSize, allocationDataToVerify.unifiedMemoryProperties);
        for (auto &cachedAllocation : testDataset) {
            EXPECT_NE(allocationNotFromCache, cachedAllocation.allocation);
        }
        svmManager->freeSVMAlloc(allocationFromCache);
        svmManager->freeSVMAlloc(allocationNotFromCache);

        svmManager->trimUSMHostAllocCache();
        ASSERT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 0u);
    }
}

TEST_F(SvmHostAllocationCacheTest, givenHostOutOfMemoryWhenAllocatingThenCacheIsTrimmedAndAllocationSucceeds) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    device->injectMemoryManager(new MockMemoryManagerWithCapacity(*device->getExecutionEnvironment()));
    MockMemoryManagerWithCapacity *memoryManager = static_cast<MockMemoryManagerWithCapacity *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    memoryManager->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    ASSERT_NE(nullptr, svmManager->usmHostAllocationsCache);

    memoryManager->capacity = MemoryConstants::pageSize64k * 3;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);

    auto allocationInCache = svmManager->createHostUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties);
    auto allocationInCache2 = svmManager->createHostUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties);
    auto allocationInCache3 = svmManager->createHostUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties);
    ASSERT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 0u);
    svmManager->freeSVMAlloc(allocationInCache);
    svmManager->freeSVMAlloc(allocationInCache2);
    svmManager->freeSVMAllocDefer(allocationInCache3);

    ASSERT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 3u);
    ASSERT_NE(svmManager->getSVMAlloc(allocationInCache), nullptr);
    ASSERT_NE(svmManager->getSVMAlloc(allocationInCache2), nullptr);
    ASSERT_NE(svmManager->getSVMAlloc(allocationInCache3), nullptr);
    auto ptr = svmManager->createHostUnifiedMemoryAllocation(MemoryConstants::pageSize64k * 2, unifiedMemoryProperties);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 0u);
    svmManager->freeSVMAlloc(ptr);

    svmManager->cleanupUSMAllocCaches();
    EXPECT_EQ(nullptr, svmManager->usmHostAllocationsCache);
}

TEST_F(SvmHostAllocationCacheTest, givenAllocationInUsageWhenAllocatingAfterFreeThenDoNotReuseAllocation) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    memoryManager->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    EXPECT_NE(nullptr, svmManager->usmHostAllocationsCache);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto allocation = svmManager->createHostUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    EXPECT_NE(allocation, nullptr);
    svmManager->freeSVMAlloc(allocation);
    EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 1u);

    memoryManager->deferAllocInUse = true;
    auto testedAllocation = svmManager->createHostUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 1u);
    auto svmData = svmManager->getSVMAlloc(testedAllocation);
    EXPECT_NE(nullptr, svmData);

    svmManager->freeSVMAlloc(testedAllocation);
    EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 2u);

    svmManager->cleanupUSMAllocCaches();
}

TEST_F(SvmHostAllocationCacheTest, givenAllocationMarkedCompletedWhenAllocatingAfterFreeThenDoNotCallAllocInUse) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    memoryManager->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    EXPECT_NE(nullptr, svmManager->usmHostAllocationsCache);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto allocation = svmManager->createHostUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    EXPECT_NE(allocation, nullptr);
    EXPECT_EQ(0u, memoryManager->waitForEnginesCompletionCalled);
    svmManager->freeSVMAlloc(allocation, true);
    EXPECT_EQ(1u, memoryManager->waitForEnginesCompletionCalled);
    EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 1u);

    auto &svmAllocCacheInfo = svmManager->usmHostAllocationsCache->allocations[0];
    EXPECT_TRUE(svmAllocCacheInfo.completed);

    memoryManager->deferAllocInUse = true;
    auto testedAllocation = svmManager->createHostUnifiedMemoryAllocation(10u, unifiedMemoryProperties);
    EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 0u);
    auto svmData = svmManager->getSVMAlloc(testedAllocation);
    EXPECT_EQ(0u, memoryManager->allocInUseCalled);
    EXPECT_NE(nullptr, svmData);

    svmManager->freeSVMAlloc(testedAllocation);
    EXPECT_EQ(svmManager->usmHostAllocationsCache->allocations.size(), 1u);

    svmManager->cleanupUSMAllocCaches();
}

TEST_F(SvmHostAllocationCacheTest, givenAllocationsInReuseWhenTrimOldAllocsCalledThenTrimAllocationsSavedBeforeTimePointLargestFirst) {
    auto deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    DebugManagerStateRestore restore;
    debugManager.flags.ExperimentalEnableHostAllocationCache.set(1);
    auto device = deviceFactory->rootDevices[0];
    auto memoryManager = reinterpret_cast<MockMemoryManager *>(device->getMemoryManager());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);
    memoryManager->usmReuseInfo.init(1 * MemoryConstants::gigaByte, UsmReuseInfo::notLimited);
    svmManager->initUsmAllocationsCaches(*device);
    EXPECT_NE(nullptr, svmManager->usmHostAllocationsCache);

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto allocation = svmManager->createUnifiedMemoryAllocation(1 * MemoryConstants::pageSize64k, unifiedMemoryProperties);
    auto allocation2 = svmManager->createUnifiedMemoryAllocation(2 * MemoryConstants::pageSize64k, unifiedMemoryProperties);
    auto allocation3 = svmManager->createUnifiedMemoryAllocation(3 * MemoryConstants::pageSize64k, unifiedMemoryProperties);
    EXPECT_NE(allocation, nullptr);
    EXPECT_NE(allocation2, nullptr);
    EXPECT_NE(allocation3, nullptr);
    svmManager->freeSVMAlloc(allocation);
    svmManager->freeSVMAlloc(allocation2);
    svmManager->freeSVMAlloc(allocation3);
    EXPECT_EQ(3u, svmManager->usmHostAllocationsCache->allocations.size());
    EXPECT_EQ(1 * MemoryConstants::pageSize64k, svmManager->usmHostAllocationsCache->allocations[0].allocationSize);
    EXPECT_EQ(2 * MemoryConstants::pageSize64k, svmManager->usmHostAllocationsCache->allocations[1].allocationSize);
    EXPECT_EQ(3 * MemoryConstants::pageSize64k, svmManager->usmHostAllocationsCache->allocations[2].allocationSize);

    auto baseTimePoint = std::chrono::high_resolution_clock::now();
    auto timeDiff = std::chrono::microseconds(1);

    svmManager->usmHostAllocationsCache->allocations[0].saveTime = baseTimePoint;
    svmManager->usmHostAllocationsCache->allocations[1].saveTime = baseTimePoint + timeDiff * 2;
    svmManager->usmHostAllocationsCache->allocations[2].saveTime = baseTimePoint + timeDiff;

    svmManager->usmHostAllocationsCache->trimOldAllocs(baseTimePoint + timeDiff, false);
    EXPECT_EQ(2u, svmManager->usmHostAllocationsCache->allocations.size());
    EXPECT_EQ(1 * MemoryConstants::pageSize64k, svmManager->usmHostAllocationsCache->allocations[0].allocationSize);
    EXPECT_EQ(2 * MemoryConstants::pageSize64k, svmManager->usmHostAllocationsCache->allocations[1].allocationSize);

    svmManager->usmHostAllocationsCache->trimOldAllocs(baseTimePoint + timeDiff, false);
    EXPECT_EQ(1u, svmManager->usmHostAllocationsCache->allocations.size());
    EXPECT_EQ(2 * MemoryConstants::pageSize64k, svmManager->usmHostAllocationsCache->allocations[0].allocationSize);

    svmManager->usmHostAllocationsCache->trimOldAllocs(baseTimePoint + timeDiff, false);
    EXPECT_EQ(1u, svmManager->usmHostAllocationsCache->allocations.size());
    EXPECT_EQ(baseTimePoint + timeDiff * 2, svmManager->usmHostAllocationsCache->allocations[0].saveTime);

    svmManager->cleanupUSMAllocCaches();
}
} // namespace NEO
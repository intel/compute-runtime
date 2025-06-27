/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_prelim_fixtures.h"
#include "shared/test/common/os_interface/linux/drm_mock_cache_info.h"
#include "shared/test/common/os_interface/linux/drm_mock_memory_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

namespace NEO {

struct BuffersWithClMemCacheClosTests : public DrmMemoryManagerLocalMemoryPrelimTest {
    void SetUp() override {
        DrmMemoryManagerLocalMemoryPrelimTest::SetUp();

        auto memoryInfo = new MockExtendedMemoryInfo(*mock);

        mock->memoryInfo.reset(memoryInfo);

        CacheReservationParameters l2CacheParameters{};
        CacheReservationParameters l3CacheParameters{};
        l3CacheParameters.maxSize = 1024;
        l3CacheParameters.maxNumRegions = 2;
        l3CacheParameters.maxNumWays = 32;
        mock->cacheInfo.reset(new MockCacheInfo(*mock->getIoctlHelper(), l2CacheParameters, l3CacheParameters));

        auto &multiTileArchInfo = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getMutableHardwareInfo()->gtSystemInfo.MultiTileArchInfo;
        multiTileArchInfo.TileCount = (memoryInfo->getDrmRegionInfos().size() - 1);
        multiTileArchInfo.IsValid = (multiTileArchInfo.TileCount > 0);

        mock->engineInfoQueried = false;
        mock->queryEngineInfo();

        clDevice = std::make_unique<MockClDevice>(device.get());
        context = std::make_unique<MockContext>(clDevice.get());
    }

    void TearDown() override {
        device.release();
        DrmMemoryManagerLocalMemoryPrelimTest::TearDown();
    }

    std::unique_ptr<MockClDevice> clDevice;
    std::unique_ptr<MockContext> context;
};

XE_HPC_CORETEST_F(BuffersWithClMemCacheClosTests, givenDrmBufferWhenItIsCreatedWithCacheClosSpecifiedInMemoryPropertiesThenSetCacheRegionInBufferObjects) {
    constexpr size_t bufferSize = MemoryConstants::pageSize;
    cl_int retVal = CL_INVALID_VALUE;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, allocflags, device.get());
    memoryProperties.memCacheClos = 1;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(context.get(), memoryProperties, flags, flagsIntel, bufferSize, nullptr, retVal));

    auto allocation = static_cast<DrmAllocation *>(buffer->getMultiGraphicsAllocation().getDefaultGraphicsAllocation());
    for (auto bo : allocation->getBOs()) {
        if (bo != nullptr) {
            EXPECT_EQ(CacheRegion::region1, bo->peekCacheRegion());
        }
    }
}

XE_HPC_CORETEST_F(BuffersWithClMemCacheClosTests, givenDrmBufferWhenItIsCreatedWithIncorrectCacheClosSpecifiedInMemoryPropertiesThenReturnNull) {

    constexpr size_t bufferSize = MemoryConstants::pageSize;
    cl_int retVal = CL_INVALID_VALUE;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, allocflags, device.get());
    memoryProperties.memCacheClos = 0xFFFF;

    auto buffer = Buffer::create(context.get(), memoryProperties, flags, flagsIntel, bufferSize, nullptr, retVal);
    EXPECT_EQ(nullptr, buffer);
}

XE_HPC_CORETEST_F(BuffersWithClMemCacheClosTests, givenDrmBuffersWhenTheyAreCreatedWithDifferentCacheClosRegionsThenSetCorrectCacheRegionsInBufferObjects) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ClosEnabled.set(1);

    constexpr size_t bufferSize = MemoryConstants::pageSize;
    cl_int retVal = CL_INVALID_VALUE;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, allocflags, device.get());

    auto &productHelper = device->getProductHelper();
    auto numCacheRegions = productHelper.getNumCacheRegions();
    EXPECT_EQ(3u, numCacheRegions);

    for (uint32_t cacheRegion = 0; cacheRegion < numCacheRegions; cacheRegion++) {
        memoryProperties.memCacheClos = cacheRegion;
        auto buffer = std::unique_ptr<Buffer>(Buffer::create(context.get(), memoryProperties, flags, flagsIntel, bufferSize, nullptr, retVal));
        EXPECT_NE(nullptr, buffer);

        auto allocation = static_cast<DrmAllocation *>(buffer->getMultiGraphicsAllocation().getDefaultGraphicsAllocation());
        for (auto bo : allocation->getBOs()) {
            if (bo != nullptr) {
                EXPECT_EQ(static_cast<CacheRegion>(cacheRegion), bo->peekCacheRegion());
            }
        }
    }
}

using UnifiedMemoryWithClMemCacheClosTests = BuffersWithClMemCacheClosTests;

XE_HPC_CORETEST_F(UnifiedMemoryWithClMemCacheClosTests, givenDrmUnifiedSharedMemoryWhenItIsCreatedWithCacheClosSpecifiedInMemoryPropertiesThenSetCacheRegionInBufferObjects) {
    AllocationProperties gpuProperties{0u,
                                       MemoryConstants::pageSize64k,
                                       AllocationType::unifiedSharedMemory,
                                       1u};
    gpuProperties.cacheRegion = 1;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);
    EXPECT_NE(allocation, nullptr);

    auto bos = static_cast<DrmAllocation *>(allocation)->getBOs();
    for (auto bo : bos) {
        if (bo != nullptr) {
            EXPECT_EQ(CacheRegion::region1, bo->peekCacheRegion());
        }
    }

    memoryManager->freeGraphicsMemory(allocation);
}

XE_HPC_CORETEST_F(UnifiedMemoryWithClMemCacheClosTests, givenDrmUnifiedSharedMemoryWhenItIsCreatedWithIncorrectCacheClosSpecifiedInMemoryPropertiesThenReturnNull) {
    AllocationProperties gpuProperties{0u,
                                       MemoryConstants::pageSize64k,
                                       AllocationType::unifiedSharedMemory,
                                       1u};
    gpuProperties.cacheRegion = 0xFFFF;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);
    EXPECT_EQ(allocation, nullptr);
}

using HostPtrAllocationWithClMemCacheClosTests = UnifiedMemoryWithClMemCacheClosTests;

XE_HPC_CORETEST_F(HostPtrAllocationWithClMemCacheClosTests, givenDrmAllocationWithHostPtrWhenItIsCreatedWithCacheClosSpecifiedInMemoryPropertiesThenSetCacheRegionInBufferObjects) {
    auto ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize * 10;
    auto properties = MockAllocationProperties{0, false, size};
    properties.cacheRegion = 1;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, ptr);
    EXPECT_NE(allocation, nullptr);
    EXPECT_EQ(3u, allocation->fragmentsStorage.fragmentCount);

    for (int i = 0; i < maxFragmentsCount; i++) {
        auto bo = static_cast<OsHandleLinux *>(allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage)->bo;
        EXPECT_EQ(CacheRegion::region1, bo->peekCacheRegion());
    }

    memoryManager->freeGraphicsMemory(allocation);
}

XE_HPC_CORETEST_F(HostPtrAllocationWithClMemCacheClosTests, givenDrmAllocationWithHostPtrWhenItIsCreatedWithIncorrectCacheClosSpecifiedInMemoryPropertiesThenReturnNull) {
    auto ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize * 10;
    auto properties = MockAllocationProperties{0, false, size};
    properties.cacheRegion = 0xFFFF;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, ptr);
    EXPECT_EQ(allocation, nullptr);
}

} // namespace NEO

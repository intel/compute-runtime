/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_prelim_fixtures.h"
#include "shared/test/common/os_interface/linux/drm_mock_cache_info.h"
#include "shared/test/common/os_interface/linux/drm_mock_memory_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"
#include "hw_cmds_xe_hpc_core_base.h"

namespace NEO {

struct BuffersWithClMemCacheClosTests : public DrmMemoryManagerLocalMemoryPrelimTest {
    void SetUp() override {
        DrmMemoryManagerLocalMemoryPrelimTest::SetUp();

        auto memoryInfo = new MockExtendedMemoryInfo(*mock);

        mock->memoryInfo.reset(memoryInfo);
        mock->cacheInfo.reset(new MockCacheInfo(*mock, 1024, 2, 32));

        auto &multiTileArchInfo = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getMutableHardwareInfo()->gtSystemInfo.MultiTileArchInfo;
        multiTileArchInfo.TileCount = (memoryInfo->getDrmRegionInfos().size() - 1);
        multiTileArchInfo.IsValid = (multiTileArchInfo.TileCount > 0);

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
            EXPECT_EQ(CacheRegion::Region1, bo->peekCacheRegion());
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
    DebugManager.flags.ClosEnabled.set(1);

    constexpr size_t bufferSize = MemoryConstants::pageSize;
    cl_int retVal = CL_INVALID_VALUE;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, allocflags, device.get());

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto numCacheRegions = gfxCoreHelper.getNumCacheRegions();
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
                                       AllocationType::UNIFIED_SHARED_MEMORY,
                                       1u};
    gpuProperties.cacheRegion = 1;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);
    EXPECT_NE(allocation, nullptr);

    auto bos = static_cast<DrmAllocation *>(allocation)->getBOs();
    for (auto bo : bos) {
        if (bo != nullptr) {
            EXPECT_EQ(CacheRegion::Region1, bo->peekCacheRegion());
        }
    }

    memoryManager->freeGraphicsMemory(allocation);
}

XE_HPC_CORETEST_F(UnifiedMemoryWithClMemCacheClosTests, givenDrmUnifiedSharedMemoryWhenItIsCreatedWithIncorrectCacheClosSpecifiedInMemoryPropertiesThenReturnNull) {
    AllocationProperties gpuProperties{0u,
                                       MemoryConstants::pageSize64k,
                                       AllocationType::UNIFIED_SHARED_MEMORY,
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
        EXPECT_EQ(CacheRegion::Region1, bo->peekCacheRegion());
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

/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/linux/drm_mock_cache_info.h"
#include "shared/test/common/os_interface/linux/drm_mock_extended.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cache/linux/cache_reservation_impl_prelim.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

using namespace NEO;

struct IsCacheReservationSupported {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (IGFX_DG1 == productFamily) {
            return false;
        } else {
            return true;
        }
    }
};

namespace L0 {
namespace ult {

class MockCacheReservationImpl : public CacheReservationImpl {
  public:
    using CacheReservationImpl::reservedL3CacheRegion;
    using CacheReservationImpl::reservedL3CacheSize;
};

class CacheReservationFixture : public DeviceFixture {
  public:
    void setUp() {
        DeviceFixture::setUp();

        auto &rootDeviceEnvironment{*neoDevice->executionEnvironment->rootDeviceEnvironments[0]};
        const PRODUCT_FAMILY productFamily{rootDeviceEnvironment.getHardwareInfo()->platform.eProductFamily};

        mockDrm = new DrmMockExtended(rootDeviceEnvironment);
        mockDrm->ioctlHelper = IoctlHelper::getI915Helper(productFamily, "2.0", *mockDrm);
        mockDrm->ioctlHelper->initialize();

        mockDrm->l3CacheInfo.reset(new MockCacheInfo(*mockDrm->ioctlHelper, 1024, 1, 32));
        rootDeviceEnvironment.osInterface.reset(new NEO::OSInterface);
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
        rootDeviceEnvironment.initGmm();

        cache = static_cast<DeviceImp *>(device)->cacheReservation.get();
        ASSERT_NE(nullptr, cache);
    }
    void tearDown() {
        DeviceFixture::tearDown();
    }

    CacheReservation *cache = nullptr;
    DrmMockExtended *mockDrm = nullptr;
};

using CacheReservationTest = Test<CacheReservationFixture>;

HWTEST2_F(CacheReservationTest, GivenCacheReservationSupportedWhenCallingReserveCacheWithZeroSizeThenRemovePriorReservation, IsCacheReservationSupported) {
    size_t cacheLevel = 3;
    size_t cacheReservationSize = 0;

    auto result = cache->reserveCache(cacheLevel, cacheReservationSize);
    EXPECT_TRUE(result);

    auto cacheImpl = static_cast<MockCacheReservationImpl *>(cache);
    EXPECT_EQ(CacheRegion::none, cacheImpl->reservedL3CacheRegion);
    EXPECT_EQ(0u, cacheImpl->reservedL3CacheSize);
}

HWTEST2_F(CacheReservationTest, GivenCacheReservationSupportedWhenCallingReserveCacheWithInvalidSizeThenDontReserveCacheRegion, IsCacheReservationSupported) {
    size_t cacheLevel = 3;
    size_t cacheReservationSize = 2048;
    ASSERT_GT(cacheReservationSize, mockDrm->getL3CacheInfo()->getMaxReservationCacheSize());

    auto result = cache->reserveCache(cacheLevel, cacheReservationSize);
    EXPECT_FALSE(result);

    auto cacheImpl = static_cast<MockCacheReservationImpl *>(cache);
    EXPECT_EQ(CacheRegion::none, cacheImpl->reservedL3CacheRegion);
    EXPECT_EQ(0u, cacheImpl->reservedL3CacheSize);
}

HWTEST2_F(CacheReservationTest, GivenCacheReservationSupportedWhenCallingReserveCacheWithValidSizeThenReserveCacheRegionWithGivenSize, IsCacheReservationSupported) {
    size_t cacheLevel = 3;
    size_t cacheReservationSize = 1024;
    ASSERT_LE(cacheReservationSize, mockDrm->getL3CacheInfo()->getMaxReservationCacheSize());

    auto result = cache->reserveCache(cacheLevel, cacheReservationSize);
    EXPECT_TRUE(result);

    auto cacheImpl = static_cast<MockCacheReservationImpl *>(cache);
    EXPECT_NE(CacheRegion::none, cacheImpl->reservedL3CacheRegion);
    EXPECT_EQ(1024u, cacheImpl->reservedL3CacheSize);
}

HWTEST2_F(CacheReservationTest, GivenCacheReservationSupportedWhenCallingSetCacheAdviceWithInvalidPointerThenReturnFalse, IsCacheReservationSupported) {
    size_t cacheLevel = 3;
    size_t cacheReservationSize = 1024;
    auto result = cache->reserveCache(cacheLevel, cacheReservationSize);
    EXPECT_TRUE(result);

    void *ptr = reinterpret_cast<void *>(0x123456789);
    size_t size = 16384;
    ze_cache_ext_region_t cacheRegion = ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_REGION_DEFAULT;

    result = cache->setCacheAdvice(ptr, size, cacheRegion);
    EXPECT_FALSE(result);
}

HWTEST2_F(CacheReservationTest, GivenCacheReservationSupportedWhenCallingSetCacheAdviceOnReservedRegionWithoutPriorCacheReservationThenReturnFalse, IsCacheReservationSupported) {
    auto cacheImpl = static_cast<MockCacheReservationImpl *>(cache);
    EXPECT_EQ(CacheRegion::none, cacheImpl->reservedL3CacheRegion);
    EXPECT_EQ(0u, cacheImpl->reservedL3CacheSize);

    uint64_t gpuAddress = 0x1200;
    void *ptr = reinterpret_cast<void *>(gpuAddress);
    size_t size = 16384;

    MockDrmAllocation mockAllocation(rootDeviceIndex, AllocationType::unifiedSharedMemory, MemoryPool::localMemory);
    MockBufferObject bo(rootDeviceIndex, mockDrm, 3, 0, 0, 1);
    mockAllocation.bufferObjects[0] = &bo;
    mockAllocation.setCpuPtrAndGpuAddress(ptr, gpuAddress);

    NEO::SvmAllocationData allocData(0);
    allocData.size = size;
    allocData.gpuAllocations.addAllocation(&mockAllocation);
    device->getDriverHandle()->getSvmAllocsManager()->insertSVMAlloc(allocData);

    ze_cache_ext_region_t cacheRegion = ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_RESERVE_REGION;

    auto result = cache->setCacheAdvice(ptr, size, cacheRegion);
    EXPECT_FALSE(result);
}

HWTEST2_F(CacheReservationTest, GivenCacheReservationSupportedWhenCallingSetCacheAdviceOnReservedCacheRegionThenSetCacheRegionCorrectly, IsCacheReservationSupported) {
    auto &productHelper = neoDevice->getProductHelper();
    if (productHelper.getNumCacheRegions() == 0) {
        GTEST_SKIP();
    }
    size_t cacheLevel = 3;
    size_t cacheReservationSize = 1024;
    auto result = cache->reserveCache(cacheLevel, cacheReservationSize);
    EXPECT_TRUE(result);

    uint64_t gpuAddress = 0x1200;
    void *ptr = reinterpret_cast<void *>(gpuAddress);
    size_t size = 16384;

    MockDrmAllocation mockAllocation(rootDeviceIndex, AllocationType::unifiedSharedMemory, MemoryPool::localMemory);
    MockBufferObject bo(rootDeviceIndex, mockDrm, 3, 0, 0, 1);
    mockAllocation.bufferObjects[0] = &bo;
    mockAllocation.setCpuPtrAndGpuAddress(ptr, gpuAddress);

    NEO::SvmAllocationData allocData(0);
    allocData.size = size;
    allocData.gpuAllocations.addAllocation(&mockAllocation);
    device->getDriverHandle()->getSvmAllocsManager()->insertSVMAlloc(allocData);

    ze_cache_ext_region_t cacheRegion = ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_RESERVE_REGION;

    result = cache->setCacheAdvice(ptr, size, cacheRegion);
    EXPECT_TRUE(result);

    auto svmData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);

    auto drmAllocation = static_cast<DrmAllocation *>(svmData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex()));
    for (auto bo : drmAllocation->getBOs()) {
        if (bo != nullptr) {
            EXPECT_NE(CacheRegion::defaultRegion, bo->peekCacheRegion());
            EXPECT_EQ(static_cast<MockCacheReservationImpl *>(cache)->reservedL3CacheRegion, bo->peekCacheRegion());
        }
    }
}

HWTEST2_F(CacheReservationTest, GivenCacheReservationSupportedWhenCallingSetCacheAdviceOnNonReservedCacheRegionThenSetCacheRegionCorrectly, IsCacheReservationSupported) {
    auto &productHelper = neoDevice->getProductHelper();
    if (productHelper.getNumCacheRegions() == 0) {
        GTEST_SKIP();
    }
    size_t cacheLevel = 3;
    size_t cacheReservationSize = 1024;
    auto result = cache->reserveCache(cacheLevel, cacheReservationSize);
    EXPECT_TRUE(result);

    uint64_t gpuAddress = 0x1200;
    void *ptr = reinterpret_cast<void *>(gpuAddress);
    size_t size = 16384;

    MockDrmAllocation mockAllocation(rootDeviceIndex, AllocationType::unifiedSharedMemory, MemoryPool::localMemory);
    MockBufferObject bo(rootDeviceIndex, mockDrm, 3, 0, 0, 1);
    mockAllocation.bufferObjects[0] = &bo;
    mockAllocation.setCpuPtrAndGpuAddress(ptr, gpuAddress);

    NEO::SvmAllocationData allocData(0);
    allocData.size = size;
    allocData.gpuAllocations.addAllocation(&mockAllocation);
    device->getDriverHandle()->getSvmAllocsManager()->insertSVMAlloc(allocData);

    ze_cache_ext_region_t cacheRegion = ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_NON_RESERVED_REGION;

    result = cache->setCacheAdvice(ptr, size, cacheRegion);
    EXPECT_TRUE(result);

    auto svmData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);

    auto drmAllocation = static_cast<DrmAllocation *>(svmData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex()));
    for (auto bo : drmAllocation->getBOs()) {
        if (bo != nullptr) {
            EXPECT_EQ(CacheRegion::defaultRegion, bo->peekCacheRegion());
        }
    }
}

HWTEST2_F(CacheReservationTest, GivenCacheReservationCreatedWhenCallingGetMaxCacheReservationSizeThenReturnZero, IsCacheReservationSupported) {
    EXPECT_EQ(mockDrm->getL3CacheInfo()->getMaxReservationCacheSize(), cache->getMaxCacheReservationSize());
}

} // namespace ult
} // namespace L0

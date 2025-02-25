/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
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
    using CacheReservationImpl::reservedL2CacheRegion;
    using CacheReservationImpl::reservedL2CacheSize;
    using CacheReservationImpl::reservedL3CacheRegion;
    using CacheReservationImpl::reservedL3CacheSize;
    using CacheReservationImpl::setCacheAdviceImpl;
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

        CacheReservationParameters l2CacheParameters{};
        l2CacheParameters.maxSize = 1;
        l2CacheParameters.maxNumRegions = 1;
        l2CacheParameters.maxNumWays = 2;
        CacheReservationParameters l3CacheParameters{};
        l3CacheParameters.maxSize = 1024;
        l3CacheParameters.maxNumRegions = 1;
        l3CacheParameters.maxNumWays = 32;
        mockDrm->cacheInfo.reset(new MockCacheInfo(*mockDrm->ioctlHelper, l2CacheParameters, l3CacheParameters));
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

HWTEST2_F(CacheReservationTest, GivenCacheReservationSupportedWhenReservingL3CacheThenReservationIsAcquiredAndReleasedAppropriately, IsCacheReservationSupported) {
    constexpr size_t cacheLevel{3U};
    constexpr size_t cacheReservationSize{128U};

    auto result1 = cache->reserveCache(cacheLevel, cacheReservationSize);
    EXPECT_TRUE(result1);

    auto cacheImpl = static_cast<MockCacheReservationImpl *>(cache);
    EXPECT_EQ(CacheRegion::region1, cacheImpl->reservedL3CacheRegion);
    EXPECT_EQ(cacheReservationSize, cacheImpl->reservedL3CacheSize);

    auto result2 = cache->reserveCache(cacheLevel, 0U);
    EXPECT_TRUE(result2);
    EXPECT_EQ(CacheRegion::none, cacheImpl->reservedL3CacheRegion);
    EXPECT_EQ(0u, cacheImpl->reservedL3CacheSize);
}

HWTEST2_F(CacheReservationTest, GivenCacheReservationSupportedWhenReservingCacheMoreThanOnceThenOnlyTheFirtstIsAcquired, IsCacheReservationSupported) {
    constexpr size_t cacheLevel{3U};
    constexpr size_t cacheReservationSize1{128U};
    constexpr size_t cacheReservationSize2{64U};

    auto result1 = cache->reserveCache(cacheLevel, cacheReservationSize1);
    EXPECT_TRUE(result1);
    auto cacheImpl = static_cast<MockCacheReservationImpl *>(cache);
    EXPECT_EQ(CacheRegion::region1, cacheImpl->reservedL3CacheRegion);
    EXPECT_EQ(cacheReservationSize1, cacheImpl->reservedL3CacheSize);

    cacheImpl->reservedL3CacheSize = 0U; // verification should fail on check for cache-region
    auto result2 = cache->reserveCache(cacheLevel, cacheReservationSize2);
    EXPECT_FALSE(result2);
    EXPECT_EQ(CacheRegion::region1, cacheImpl->reservedL3CacheRegion);
    cacheImpl->reservedL3CacheSize = cacheReservationSize1;

    cacheImpl->reservedL3CacheRegion = CacheRegion::none; // verification should fail on check for reserved size
    auto result3 = cache->reserveCache(cacheLevel, cacheReservationSize2);
    EXPECT_FALSE(result3);
    EXPECT_EQ(cacheReservationSize1, cacheImpl->reservedL3CacheSize);
    cacheImpl->reservedL3CacheRegion = CacheRegion::region1;

    auto result4 = cache->reserveCache(cacheLevel, 0U);
    EXPECT_TRUE(result4);
    EXPECT_EQ(CacheRegion::none, cacheImpl->reservedL3CacheRegion);
    EXPECT_EQ(0u, cacheImpl->reservedL3CacheSize);
}

HWTEST2_F(CacheReservationTest, GivenCacheReservationSupportedWhenReservingL2CacheThenReservationIsAcquiredAndReleasedAppropriately, IsCacheReservationSupported) {
    constexpr size_t cacheLevel{2U};
    constexpr size_t cacheReservationSize1{1U};

    mockDrm->closIndex = 2U; // force next returned closIndex to be 3
    auto result1 = cache->reserveCache(cacheLevel, cacheReservationSize1);
    EXPECT_TRUE(result1);
    auto cacheImpl = static_cast<MockCacheReservationImpl *>(cache);
    EXPECT_EQ(CacheRegion::region3, cacheImpl->reservedL2CacheRegion);
    EXPECT_EQ(cacheReservationSize1, cacheImpl->reservedL2CacheSize);

    auto result3 = cache->reserveCache(cacheLevel, 0U);
    EXPECT_TRUE(result3);
    EXPECT_EQ(CacheRegion::none, cacheImpl->reservedL2CacheRegion);
    EXPECT_EQ(0u, cacheImpl->reservedL2CacheSize);
}

HWTEST2_F(CacheReservationTest, GivenCacheReservationSupportedWhenCallingReserveCacheWithInvalidSizeThenDontReserveCacheRegion, IsCacheReservationSupported) {
    uint16_t cacheLevel = 3U;
    size_t cacheReservationSize = 2048;
    ASSERT_GT(cacheReservationSize, mockDrm->getCacheInfo()->getMaxReservationCacheSize(toCacheLevel(cacheLevel)));

    auto result = cache->reserveCache(cacheLevel, cacheReservationSize);
    EXPECT_FALSE(result);

    auto cacheImpl = static_cast<MockCacheReservationImpl *>(cache);
    EXPECT_EQ(CacheRegion::none, cacheImpl->reservedL3CacheRegion);
    EXPECT_EQ(0u, cacheImpl->reservedL3CacheSize);
}

HWTEST2_F(CacheReservationTest, GivenCacheReservationSupportedWhenCallingReserveCacheWithInvalidCacheLevelThenDontReserveCacheRegion, IsCacheReservationSupported) {
    constexpr size_t cacheLevel{1U};
    constexpr size_t cacheReservationSize{128U};

    auto result = cache->reserveCache(cacheLevel, cacheReservationSize);
    EXPECT_FALSE(result);

    auto cacheImpl = static_cast<MockCacheReservationImpl *>(cache);
    EXPECT_EQ(CacheRegion::none, cacheImpl->reservedL3CacheRegion);
    EXPECT_EQ(0U, cacheImpl->reservedL3CacheSize);
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

HWTEST2_F(CacheReservationTest, GivenCacheReservationWhenCallingGetMaxCacheReservationSizeThenAppropriateValueReturned, IsCacheReservationSupported) {
    uint16_t cacheLevel{3U};
    EXPECT_EQ(mockDrm->getCacheInfo()->getMaxReservationCacheSize(toCacheLevel(cacheLevel)), cache->getMaxCacheReservationSize(cacheLevel));
    cacheLevel = 2U;
    EXPECT_EQ(mockDrm->getCacheInfo()->getMaxReservationCacheSize(toCacheLevel(cacheLevel)), cache->getMaxCacheReservationSize(cacheLevel));
    cacheLevel = 1U;
    EXPECT_EQ(0U, cache->getMaxCacheReservationSize(cacheLevel));
}

class L2CacheReservationFixture : public DeviceFixture {
  public:
    void setUp() {
        DeviceFixture::setUp();

        l2CacheParameters.maxSize = 1;
        l2CacheParameters.maxNumRegions = 1;
        l2CacheParameters.maxNumWays = 2;

        auto &rootDeviceEnvironment{*neoDevice->executionEnvironment->rootDeviceEnvironments[0]};
        mockDrm = new DrmMockExtended(rootDeviceEnvironment);
        auto mockIoctlHelper{new MockIoctlHelper{*mockDrm}};
        mockDrm->ioctlHelper.reset(mockIoctlHelper);
        mockIoctlHelper->closAllocResult = CacheRegion::region3;
        mockIoctlHelper->closAllocWaysResult = l2CacheParameters.maxNumWays;

        mockDrm->cacheInfo.reset(new MockCacheInfo(*mockDrm->ioctlHelper, l2CacheParameters, l3CacheParameters));
        rootDeviceEnvironment.osInterface.reset(new NEO::OSInterface);
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));

        cache = reinterpret_cast<MockCacheReservationImpl *>(static_cast<DeviceImp *>(device)->cacheReservation.get());
        ASSERT_NE(nullptr, cache);
    }
    void tearDown() {
        DeviceFixture::tearDown();
    }

    CacheReservationParameters l2CacheParameters{};
    CacheReservationParameters l3CacheParameters{};
    MockCacheReservationImpl *cache{nullptr};
    DrmMockExtended *mockDrm{nullptr};
};
using L2CacheReservationTest = Test<L2CacheReservationFixture>;

HWTEST2_F(L2CacheReservationTest, GivenCacheReservationSupportedWhenCallingSetCacheAdviceOnL2RegionReservedThenSetCacheRegionCorrectly, IsCacheReservationSupported) {
    auto &productHelper = neoDevice->getProductHelper();
    if (productHelper.getNumCacheRegions() == 0) {
        GTEST_SKIP();
    }

    constexpr size_t cacheLevel{2UL};
    EXPECT_TRUE(cache->reserveCache(cacheLevel, l2CacheParameters.maxSize));

    constexpr uint64_t gpuAddress{0x1200};
    void *ptr{reinterpret_cast<void *>(gpuAddress)};
    constexpr size_t size{16384UL};

    MockDrmAllocation mockAllocation{rootDeviceIndex, AllocationType::unifiedSharedMemory, MemoryPool::localMemory};
    MockBufferObject bo{rootDeviceIndex, mockDrm, 3, 0, 0, 1};
    mockAllocation.bufferObjects[0] = &bo;
    mockAllocation.setCpuPtrAndGpuAddress(ptr, gpuAddress);

    NEO::SvmAllocationData allocData{0};
    allocData.size = size;
    allocData.gpuAllocations.addAllocation(&mockAllocation);
    device->getDriverHandle()->getSvmAllocsManager()->insertSVMAlloc(allocData);

    constexpr auto l2CacheRegionId{1U + static_cast<uint32_t>(ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_NON_RESERVED)};
    EXPECT_TRUE(cache->setCacheAdviceImpl(ptr, size, l2CacheRegionId));

    auto svmData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);

    auto drmAllocation = static_cast<DrmAllocation *>(svmData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex()));
    for (auto bo : drmAllocation->getBOs()) {
        if (bo != nullptr) {
            EXPECT_EQ(CacheRegion::region3, bo->peekCacheRegion());
            EXPECT_EQ(static_cast<MockCacheReservationImpl *>(cache)->reservedL2CacheRegion, bo->peekCacheRegion());
        }
    }
}

} // namespace ult
} // namespace L0

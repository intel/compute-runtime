/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cache/linux/cache_reservation_impl.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

using namespace NEO;

namespace L0 {
namespace ult {

class CacheReservationFixture : public DeviceFixture {
  public:
    void setUp() {
        DeviceFixture::setUp();
        auto deviceImp = static_cast<DeviceImp *>(device);
        ASSERT_NE(nullptr, deviceImp->cacheReservation.get());

        cache = deviceImp->cacheReservation.get();
    }
    void tearDown() {
        DeviceFixture::tearDown();
    }
    CacheReservation *cache = nullptr;
};

using CacheReservationTest = Test<CacheReservationFixture>;

TEST_F(CacheReservationTest, GivenCacheReservationCreatedWhenCallingReserveCacheThenReturnFalse) {
    size_t cacheLevel = 3;
    size_t cacheReservationSize = 1024;

    auto result = cache->reserveCache(cacheLevel, cacheReservationSize);
    EXPECT_FALSE(result);
}

TEST_F(CacheReservationTest, GivenCacheReservationCreatedWhenCallingSetCacheAdviceThenReturnFalse) {
    void *ptr = reinterpret_cast<void *>(0x123456789);
    size_t regionSize = 512;
    ze_cache_ext_region_t cacheRegion = ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_REGION_DEFAULT;

    auto result = cache->setCacheAdvice(ptr, regionSize, cacheRegion);
    EXPECT_FALSE(result);
}

TEST_F(CacheReservationTest, GivenCacheReservationCreatedWhenCallingGetMaxCacheReservationSizeThenReturnZero) {
    EXPECT_EQ(0u, cache->getMaxCacheReservationSize());
}
} // namespace ult
} // namespace L0

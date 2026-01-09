/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cache/cache_reservation.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

using namespace NEO;

namespace L0 {
namespace ult {

class CacheReservationFixture : public DeviceFixture {
  public:
    void setUp() {
        DeviceFixture::setUp();
        auto l0Device = static_cast<Device *>(device);
        ASSERT_NE(nullptr, l0Device->cacheReservation.get());
        cache = l0Device->cacheReservation.get();
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
    constexpr auto cacheLevel{3U};
    EXPECT_EQ(0u, cache->getMaxCacheReservationSize(cacheLevel));
}

} // namespace ult
} // namespace L0

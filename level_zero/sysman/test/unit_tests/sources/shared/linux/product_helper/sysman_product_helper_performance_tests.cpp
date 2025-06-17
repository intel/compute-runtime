/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/performance/linux/mock_sysfs_performance_prelim.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanProductHelperPerformanceTest = ::testing::Test;

HWTEST2_F(SysmanProductHelperPerformanceTest, GivenMediaPerformanceFactorWhenGettingMediaPerformanceMultiplierForProductsAtMostDg2ThenValidMultiplierIsReturned, IsAtMostDg2) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);

    double performanceFactor = 0;
    double multiplier = 0;
    pSysmanProductHelper->getMediaPerformanceFactorMultiplier(performanceFactor, &multiplier);
    EXPECT_EQ(0, multiplier);

    performanceFactor = 30;
    pSysmanProductHelper->getMediaPerformanceFactorMultiplier(performanceFactor, &multiplier);
    EXPECT_EQ(0.5, multiplier);

    performanceFactor = 50;
    pSysmanProductHelper->getMediaPerformanceFactorMultiplier(performanceFactor, &multiplier);
    EXPECT_EQ(0.5, multiplier);

    performanceFactor = 100;
    pSysmanProductHelper->getMediaPerformanceFactorMultiplier(performanceFactor, &multiplier);
    EXPECT_EQ(1, multiplier);
}

HWTEST2_F(SysmanProductHelperPerformanceTest, GivenMediaPerformanceFactorWhenGettingMediaPerformanceMultiplierForProductFamilyIsPVCThenValidMultiplierIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);

    double performanceFactor = 0;
    double multiplier = 0;
    pSysmanProductHelper->getMediaPerformanceFactorMultiplier(performanceFactor, &multiplier);
    EXPECT_EQ(0.5, multiplier);

    performanceFactor = 50;
    pSysmanProductHelper->getMediaPerformanceFactorMultiplier(performanceFactor, &multiplier);
    EXPECT_EQ(0.5, multiplier);

    performanceFactor = 100;
    pSysmanProductHelper->getMediaPerformanceFactorMultiplier(performanceFactor, &multiplier);
    EXPECT_EQ(1, multiplier);
}

HWTEST2_F(SysmanProductHelperPerformanceTest, GivenPerformanceModuleWhenQueryingPerfFactorSupportedThenPerfFactorIsSupported, IsXeCore) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(true, pSysmanProductHelper->isPerfFactorSupported());
}

} // namespace ult
} // namespace Sysman
} // namespace L0
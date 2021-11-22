/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "test.h"

using namespace NEO;

using HwInfoConfigTestWindowsDg2 = ::testing::Test;

HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, whenConvertingTimestampsToCsDomainThenNothingIsChanged, IGFX_DG2);
DG2TEST_F(HwInfoConfigTestWindowsDg2, whenConvertingTimestampsToCsDomainThenGpuTicksAreShifted) {
    auto hwInfoConfig = HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    uint64_t gpuTicks = 0x12345u;

    auto expectedGpuTicks = gpuTicks >> 1;

    hwInfoConfig->convertTimestampsFromOaToCsDomain(gpuTicks);
    EXPECT_EQ(expectedGpuTicks, gpuTicks);
}

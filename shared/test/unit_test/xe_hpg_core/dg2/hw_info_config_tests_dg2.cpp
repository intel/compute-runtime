/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using HwInfoConfigTestDg2 = ::testing::Test;

DG2TEST_F(HwInfoConfigTestDg2, whenConvertingTimestampsToCsDomainThenGpuTicksAreShifted) {
    auto hwInfoConfig = HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    uint64_t gpuTicks = 0x12345u;

    auto expectedGpuTicks = gpuTicks >> 1;

    hwInfoConfig->convertTimestampsFromOaToCsDomain(gpuTicks);
    EXPECT_EQ(expectedGpuTicks, gpuTicks);
}
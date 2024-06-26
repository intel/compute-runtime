/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/test_macros/test.h"
using namespace NEO;

TEST(HwInfoTest, givenReleaseHelperWithL3BankConfigWhenSetupDefaultHwInfoThenL3ConfigIsProperlySet) {
    HardwareInfo hwInfo{};
    MockReleaseHelper releaseHelper;

    releaseHelper.getL3BankCountResult = 0;
    releaseHelper.getL3CacheBankSizeInKbResult = 0;

    setupDefaultGtSysInfo(&hwInfo, &releaseHelper);

    EXPECT_EQ_VAL(1u, hwInfo.gtSystemInfo.L3BankCount);
    EXPECT_EQ_VAL(1u, hwInfo.gtSystemInfo.L3CacheSizeInKb);

    releaseHelper.getL3BankCountResult = 0;
    releaseHelper.getL3CacheBankSizeInKbResult = 4;

    setupDefaultGtSysInfo(&hwInfo, &releaseHelper);

    EXPECT_EQ_VAL(1u, hwInfo.gtSystemInfo.L3BankCount);
    EXPECT_EQ_VAL(4u, hwInfo.gtSystemInfo.L3CacheSizeInKb);

    releaseHelper.getL3BankCountResult = 2;
    releaseHelper.getL3CacheBankSizeInKbResult = 0;

    setupDefaultGtSysInfo(&hwInfo, &releaseHelper);

    EXPECT_EQ_VAL(2u, hwInfo.gtSystemInfo.L3BankCount);
    EXPECT_EQ_VAL(1u, hwInfo.gtSystemInfo.L3CacheSizeInKb);

    releaseHelper.getL3BankCountResult = 3;
    releaseHelper.getL3CacheBankSizeInKbResult = 3;

    setupDefaultGtSysInfo(&hwInfo, &releaseHelper);

    EXPECT_EQ_VAL(3u, hwInfo.gtSystemInfo.L3BankCount);
    EXPECT_EQ_VAL(9u, hwInfo.gtSystemInfo.L3CacheSizeInKb);
}

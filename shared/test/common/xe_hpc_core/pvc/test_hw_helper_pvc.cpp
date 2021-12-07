/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/hw_helper_tests.h"

#include "test.h"

using namespace NEO;
using HwHelperTestPvc = ::testing::Test;

PVCTEST_F(HwHelperTestPvc, givenSlmSizeWhenEncodingThenReturnCorrectValues) {
    ComputeSlmTestInput computeSlmValuesPvcAndLaterTestsInput[] = {
        {0, 0 * KB},
        {1, 0 * KB + 1},
        {1, 1 * KB},
        {2, 1 * KB + 1},
        {2, 2 * KB},
        {3, 2 * KB + 1},
        {3, 4 * KB},
        {4, 4 * KB + 1},
        {4, 8 * KB},
        {5, 8 * KB + 1},
        {5, 16 * KB},
        {8, 16 * KB + 1},
        {8, 24 * KB},
        {6, 24 * KB + 1},
        {6, 32 * KB},
        {9, 32 * KB + 1},
        {9, 48 * KB},
        {7, 48 * KB + 1},
        {7, 64 * KB},
        {10, 64 * KB + 1},
        {10, 96 * KB},
        {11, 96 * KB + 1},
        {11, 128 * KB}};

    auto hwInfo = *defaultHwInfo;
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    for (auto &testInput : computeSlmValuesPvcAndLaterTestsInput) {
        EXPECT_EQ(testInput.expected, hwHelper.computeSlmValues(hwInfo, testInput.slmSize));
    }

    EXPECT_THROW(hwHelper.computeSlmValues(hwInfo, 129 * KB), std::exception);
}

PVCTEST_F(HwHelperTestPvc, WhenGettingIsCpuImageTransferPreferredThenTrueIsReturned) {
    auto &hwHelper = HwHelper::get(renderCoreFamily);
    EXPECT_TRUE(hwHelper.isCpuImageTransferPreferred(*defaultHwInfo));
}

PVCTEST_F(HwHelperTestPvc, givenHwHelperWhenGettingISAPaddingThenCorrectValueIsReturned) {
    auto &hwHelper = NEO::HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    EXPECT_EQ(hwHelper.getPaddingForISAAllocation(), 3584u);
}

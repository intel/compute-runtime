/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;
using HwHelperTestDg2 = ::testing::Test;

DG2TEST_F(HwHelperTestDg2, GivenDifferentSteppingWhenComputeSlmSizeIsCalledThenCorrectValueIsReturned) {
    using SHARED_LOCAL_MEMORY_SIZE = typename FamilyType::INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;

    __REVID revisions[] = {REVISION_A0, REVISION_B};
    auto &hwHelper = HwHelperHw<FamilyType>::get();
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);

    auto hwInfo = *defaultHwInfo;
    for (auto revision : revisions) {
        hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(revision, hwInfo);
        for (auto &testInput : computeSlmValuesXeHPAndLaterTestsInput) {
            EXPECT_EQ(testInput.expected, hwHelper.computeSlmValues(hwInfo, testInput.slmSize));
        }
    }
}

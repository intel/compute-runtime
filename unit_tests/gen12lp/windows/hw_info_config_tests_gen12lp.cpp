/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "unit_tests/gen12lp/special_ult_helper_gen12lp.h"
#include "unit_tests/os_interface/windows/hw_info_config_win_tests.h"

using namespace NEO;
using namespace std;

using HwInfoConfigTestWindowsGen12lp = HwInfoConfigTestWindows;

GEN12LPTEST_F(HwInfoConfigTestWindowsGen12lp, givenE2ECSetByKmdWhenConfiguringHwThenAdjustInternalImageFlag) {
    FeatureTable &localFeatureTable = outHwInfo.featureTable;

    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    localFeatureTable.ftrE2ECompression = true;
    hwInfoConfig->configureHardwareCustom(&outHwInfo, nullptr);
    bool expectedValue = SpecialUltHelperGen12lp::shouldCompressionBeEnabledAfterConfigureHardwareCustom(outHwInfo);
    EXPECT_EQ(expectedValue, outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_EQ(expectedValue, outHwInfo.capabilityTable.ftrRenderCompressedImages);

    localFeatureTable.ftrE2ECompression = false;
    hwInfoConfig->configureHardwareCustom(&outHwInfo, nullptr);
    expectedValue = SpecialUltHelperGen12lp::shouldCompressionBeEnabledAfterConfigureHardwareCustom(outHwInfo);
    EXPECT_EQ(expectedValue, outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_EQ(expectedValue, outHwInfo.capabilityTable.ftrRenderCompressedImages);
}

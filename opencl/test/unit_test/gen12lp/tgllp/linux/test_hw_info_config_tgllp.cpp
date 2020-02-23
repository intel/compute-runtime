/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/os_interface/linux/hw_info_config_linux_tests.h"

using HwInfoConfigTestLinuxTGLLP = HwInfoConfigTestLinux;

TGLLPTEST_F(HwInfoConfigTestLinuxTGLLP, givenE2ECSetWhenConfiguringHwThenAdjustInternalImageFlag) {
    FeatureTable &localFeatureTable = outHwInfo.featureTable;

    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    localFeatureTable.ftrE2ECompression = true;
    hwInfoConfig->configureHardwareCustom(&outHwInfo, nullptr);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrRenderCompressedImages);

    localFeatureTable.ftrE2ECompression = false;
    hwInfoConfig->configureHardwareCustom(&outHwInfo, nullptr);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrRenderCompressedImages);
}

/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/offline_compiler/ocloc_product_config_tests.h"

#include "shared/source/helpers/product_config_helper.h"

namespace NEO {
TEST_P(OclocProductConfigTests, GivenProductConfigValuesWhenInitHardwareInfoThenCorrectValuesAreSet) {
    auto deviceId = 0u;
    auto revId = 0u;
    auto allSupportedConfigs = mockOfflineCompiler->argHelper->productConfigHelper->getDeviceAotInfo();

    for (const auto &deviceConfig : allSupportedConfigs) {
        if (aotConfig.ProductConfig == deviceConfig.aotConfig.ProductConfig) {
            deviceId = deviceConfig.deviceIds->front();
            revId = deviceConfig.aotConfig.ProductConfigID.Revision;
            break;
        }
    }

    mockOfflineCompiler->deviceName = ProductConfigHelper::parseMajorMinorRevisionValue(aotConfig);
    mockOfflineCompiler->initHardwareInfo(mockOfflineCompiler->deviceName);

    EXPECT_EQ(mockOfflineCompiler->hwInfo.platform.eProductFamily, productFamily);
    EXPECT_EQ(mockOfflineCompiler->hwInfo.platform.usRevId, revId);
    EXPECT_EQ(mockOfflineCompiler->hwInfo.platform.usDeviceID, deviceId);
}

} // namespace NEO
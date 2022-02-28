/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/offline_compiler/ocloc_product_config_tests.h"

namespace NEO {
TEST_P(OclocProductConfigTests, GivenProductConfigValuesWhenInitHardwareInfoThenCorrectValuesAreSet) {
    auto deviceId = 0u;
    auto revId = 0u;
    auto allSupportedDeviceConfigs = mockOfflineCompiler->argHelper->getAllSupportedDeviceConfigs();

    for (const auto &deviceConfig : allSupportedDeviceConfigs) {
        if (productConfig == deviceConfig.config) {
            if (deviceConfig.deviceIds) {
                deviceId = deviceConfig.deviceIds->front();
            }
            revId = deviceConfig.revId;
            break;
        }
    }

    mockOfflineCompiler->deviceName = mockOfflineCompiler->argHelper->parseProductConfigFromValue(productConfig);
    mockOfflineCompiler->initHardwareInfo(mockOfflineCompiler->deviceName);

    EXPECT_EQ(mockOfflineCompiler->hwInfo.platform.eProductFamily, productFamily);
    EXPECT_EQ(mockOfflineCompiler->hwInfo.platform.usRevId, revId);
    EXPECT_EQ(mockOfflineCompiler->hwInfo.platform.usDeviceID, deviceId);
}

} // namespace NEO
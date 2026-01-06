/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/offline_compiler/ocloc_product_config_tests.h"

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/source/helpers/product_config_helper.h"

namespace NEO {
TEST_P(OclocProductConfigTests, GivenProductConfigValuesWhenInitHardwareInfoThenCorrectValuesAreSet) {
    auto deviceId = 0u;
    auto revId = 0u;
    auto &allSupportedConfigs = mockOfflineCompiler->argHelper->productConfigHelper->getDeviceAotInfo();

    for (const auto &deviceConfig : allSupportedConfigs) {
        if (aotConfig.value == deviceConfig.aotConfig.value) {
            deviceId = deviceConfig.deviceIds->front();
            revId = deviceConfig.aotConfig.revision;
            break;
        }
    }

    mockOfflineCompiler->deviceName = ProductConfigHelper::parseMajorMinorRevisionValue(aotConfig);
    mockOfflineCompiler->initHardwareInfo(mockOfflineCompiler->deviceName);

    EXPECT_EQ(mockOfflineCompiler->hwInfo.platform.eProductFamily, productFamily);
    EXPECT_EQ(mockOfflineCompiler->hwInfo.platform.usRevId, revId);
    EXPECT_EQ(mockOfflineCompiler->hwInfo.platform.usDeviceID, deviceId);
}

TEST_P(OclocProductConfigTests, givenProductConfigAsDeviceNameWhenInitHwInfoThenCorrectResultIsReturned) {
    MockOfflineCompiler mockOfflineCompiler;
    std::stringstream config;
    config << aotConfig.value;
    mockOfflineCompiler.deviceName = config.str();

    EXPECT_EQ(OCLOC_SUCCESS, mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName));
    EXPECT_EQ(aotConfig.value, mockOfflineCompiler.deviceConfig);
}
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(OclocProductConfigTests);
} // namespace NEO

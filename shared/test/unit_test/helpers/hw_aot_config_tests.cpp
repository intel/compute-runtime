/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/product_config_fixture.h"

using namespace NEO;

HWTEST2_P(ProductConfigHwInfoBadArchTests, givenAotConfigWithIncorrectArchWhenGetProductConfigThenUnknownIsaIsReturned, IsAtLeastMtl) {
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = productConfig;
    hwInfo.ipVersion.architecture = invalidConfig.architecture;
    hwInfo.ipVersion.release = aotConfig.release;
    hwInfo.ipVersion.revision = aotConfig.revision;
    auto ret = productHelper->getProductConfigFromHwInfo(hwInfo);
    EXPECT_EQ(ret, AOT::UNKNOWN_ISA);
}

HWTEST2_P(ProductConfigHwInfoBadRevisionTests, givenAotConfigWithIncorrectRevisionIdWhenGetProductConfigThenUnknownIsaIsReturned, IsAtLeastMtl) {
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = productConfig;
    hwInfo.ipVersion.architecture = aotConfig.architecture;
    hwInfo.ipVersion.release = aotConfig.release;
    hwInfo.ipVersion.revision = invalidConfig.revision;
    auto ret = productHelper->getProductConfigFromHwInfo(hwInfo);
    EXPECT_EQ(ret, AOT::UNKNOWN_ISA);
}

HWTEST2_P(ProductConfigHwInfoTests, givenAotConfigWhenSetHwInfoGmdIdThenCorrectValueIsSet, IsAtLeastMtl) {
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = productConfig;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &compilerProductHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    compilerProductHelper.setProductConfigForHwInfo(hwInfo, aotConfig);
    EXPECT_EQ(hwInfo.ipVersion.architecture, aotConfig.architecture);
    EXPECT_EQ(hwInfo.ipVersion.release, aotConfig.release);
    EXPECT_EQ(hwInfo.ipVersion.revision, aotConfig.revision);
}

HWTEST2_P(ProductConfigHwInfoTests, givenAotConfigWithIncorrectReleaseWhenGetProductConfigThenUnknownIsaIsReturned, IsAtLeastMtl) {
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = productConfig;
    hwInfo.ipVersion.architecture = aotConfig.architecture;
    hwInfo.ipVersion.release = invalidConfig.release;
    hwInfo.ipVersion.revision = aotConfig.revision;

    auto ret = productHelper->getProductConfigFromHwInfo(hwInfo);
    EXPECT_EQ(ret, AOT::UNKNOWN_ISA);
}

HWTEST2_P(ProductConfigHwInfoTests, givenUnknownAotConfigWhenGetProductConfigThenUnknownIsaIsReturned, IsAtLeastMtl) {
    hwInfo.ipVersion = {};
    auto ret = productHelper->getProductConfigFromHwInfo(hwInfo);
    EXPECT_EQ(ret, AOT::UNKNOWN_ISA);
}

HWTEST2_P(ProductConfigHwInfoTests, givenAotConfigWhenGetProductConfigThenCorrectValueIsReturned, IsAtLeastMtl) {
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = productConfig;
    hwInfo.ipVersion.architecture = aotConfig.architecture;
    hwInfo.ipVersion.release = aotConfig.release;
    hwInfo.ipVersion.revision = aotConfig.revision;
    auto ret = productHelper->getProductConfigFromHwInfo(hwInfo);
    EXPECT_EQ(ret, productConfig);
}
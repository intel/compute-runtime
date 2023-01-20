/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_hw_info_config.h"
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
    CompilerProductHelper::get(productFamily)->setProductConfigForHwInfo(hwInfo, aotConfig);
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
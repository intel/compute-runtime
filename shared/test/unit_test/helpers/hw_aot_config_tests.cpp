/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/product_config_fixture.h"

using namespace NEO;

HWTEST2_P(ProductConfigHwInfoTests, givenAotConfigWhenSetHwInfoGmdIdThenCorrectValueIsSet, IsAtLeastMtl) {
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = productConfig;
    compilerProductHelper->setProductConfigForHwInfo(hwInfo, aotConfig);

    EXPECT_EQ(hwInfo.ipVersion.architecture, aotConfig.architecture);
    EXPECT_EQ(hwInfo.ipVersion.release, aotConfig.release);
    EXPECT_EQ(hwInfo.ipVersion.revision, aotConfig.revision);

    auto ret = compilerProductHelper->getHwIpVersion(hwInfo);
    EXPECT_EQ(ret, productConfig);
}

HWTEST2_P(ProductConfigHwInfoTests, givenUnknownAotConfigWhenGetProductConfigThenDefaultConfigIsReturned, IsAtLeastMtl) {
    hwInfo.ipVersion = {};
    hwInfo.platform.usDeviceID = 0;
    hwInfo.platform.usRevId = CommonConstants::invalidRevisionID;
    auto ret = compilerProductHelper->getHwIpVersion(hwInfo);
    EXPECT_EQ(ret, compilerProductHelper->getDefaultHwIpVersion());
}

HWTEST2_P(ProductConfigHwInfoTests, givenAotConfigWhenGetProductConfigThenCorrectValueIsReturned, IsAtLeastMtl) {
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = productConfig;
    hwInfo.ipVersion.architecture = aotConfig.architecture;
    hwInfo.ipVersion.release = aotConfig.release;
    hwInfo.ipVersion.revision = aotConfig.revision;
    auto ret = compilerProductHelper->getHwIpVersion(hwInfo);
    EXPECT_EQ(ret, productConfig);
}
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ProductConfigHwInfoTests);

TEST(ProductConfigHwInfoTest, givenDefaultAotConfigWhenGetProductConfigThenSameValueIsReturned) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &compilerProductHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    auto hwInfo = *defaultHwInfo;
    auto ret = compilerProductHelper.getHwIpVersion(hwInfo);
    EXPECT_EQ(ret, hwInfo.ipVersion.value);
}

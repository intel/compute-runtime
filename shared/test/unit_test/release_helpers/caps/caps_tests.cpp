/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/release_helpers/caps/caps_setup.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(CapsSetupTest, givenEveryEnabledProductConfigWhenResolvingCapsThenCapsAreReturned) {
    ProductConfigHelper productConfigHelper{};
    const auto &deviceAotInfos = productConfigHelper.getDeviceAotInfo();
    ASSERT_FALSE(deviceAotInfos.empty());

    for (const auto &deviceAotInfo : deviceAotInfos) {
        EXPECT_TRUE(resolveCaps(deviceAotInfo.aotConfig).has_value());
    }
}

TEST(CapsSetupTest, givenEveryEnabledProductConfigWhenSettingUpCapsThenHwInfoIsInitializedWithResolvedCaps) {
    ProductConfigHelper productConfigHelper{};

    for (const auto &deviceAotInfo : productConfigHelper.getDeviceAotInfo()) {
        ASSERT_NE(nullptr, deviceAotInfo.hwInfo);

        HardwareInfo hwInfo = *deviceAotInfo.hwInfo;
        hwInfo.caps = {};
        hwInfo.ipVersion = deviceAotInfo.aotConfig;

        setupCaps(hwInfo);

        auto expectedCaps = resolveCaps(deviceAotInfo.aotConfig);
        ASSERT_TRUE(expectedCaps.has_value());
        EXPECT_EQ(expectedCaps->isDotProductAccumulateSystolicSupported, hwInfo.caps.isDotProductAccumulateSystolicSupported);
    }
}

TEST(CapsSetupTest, givenUnknownIpVersionWhenResolvingCapsThenNulloptIsReturned) {
    HardwareIpVersion unknownIpVersion{};
    unknownIpVersion.architecture = 0x3ff;
    unknownIpVersion.release = 0xff;
    unknownIpVersion.revision = 0x3f;

    EXPECT_FALSE(resolveCaps(unknownIpVersion).has_value());
}

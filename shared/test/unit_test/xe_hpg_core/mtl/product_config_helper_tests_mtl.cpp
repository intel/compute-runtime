/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "neo_aot_platforms.h"

using namespace NEO;
using ProductConfigHelperMtlTests = ::testing::Test;
using ProductHelperMtlTests = ProductHelperTest;

MTLTEST_F(ProductConfigHelperMtlTests, givenXeLpgReleaseWhenSearchForDeviceAcronymThenObjectIsFound) {
    auto productConfigHelper = std::make_unique<ProductConfigHelper>();
    auto aotInfos = productConfigHelper->getDeviceAotInfo();
    EXPECT_TRUE(std::any_of(aotInfos.begin(), aotInfos.end(), ProductConfigHelper::findDeviceAcronymForRelease(AOT::XE_LPG_RELEASE)));
}

MTLTEST_F(ProductConfigHelperMtlTests, givenVariousVariantsOfXeLpgAcronymsWhenGetReleaseThenCorrectValueIsReturned) {
    auto productConfigHelper = std::make_unique<ProductConfigHelper>();
    std::vector<std::string> acronymsVariants = {"xe_lpg_core", "xe_lpg", "xelpg", "XeLpg"};
    for (auto &acronym : acronymsVariants) {
        ProductConfigHelper::adjustDeviceName(acronym);
        auto ret = productConfigHelper->getReleaseFromDeviceName(acronym);
        EXPECT_EQ(ret, AOT::XE_LPG_RELEASE);
    }
}

MTLTEST_F(ProductConfigHelperMtlTests, givenMtlConfigsWhenSearchForDeviceAcronymsThenObjectIsFound) {
    auto productConfigHelper = std::make_unique<ProductConfigHelper>();
    std::vector<AOT::PRODUCT_CONFIG> mtlConfigs = {AOT::MTL_U_B0, AOT::MTL_H_B0};
    auto deviceAcronyms = productConfigHelper->getDeviceAcronyms();
    for (const auto &config : mtlConfigs) {
        auto acronym = productConfigHelper->getAcronymForProductConfig(config);
        EXPECT_NE(std::find(deviceAcronyms.begin(), deviceAcronyms.end(), acronym), deviceAcronyms.end());
    }
}

MTLTEST_F(ProductConfigHelperMtlTests, givenPubliclyAvailableAcronymsForMtlDevicesWhenGetProductConfigThenCorrectValueIsReturned) {
    auto productConfigHelper = std::make_unique<ProductConfigHelper>();
    std::vector<std::string> mtlUAcronyms = {"mtl-s", "mtl-u", "mtl-m"};
    std::vector<std::string> mtlHAcronyms = {"mtl-h", "mtl-p"};
    for (auto &acronym : mtlUAcronyms) {
        ProductConfigHelper::adjustDeviceName(acronym);
        auto ret = productConfigHelper->getProductConfigFromDeviceName(acronym);
        EXPECT_EQ(ret, AOT::MTL_U_B0);
    }
    for (auto &acronym : mtlHAcronyms) {
        ProductConfigHelper::adjustDeviceName(acronym);
        auto ret = productConfigHelper->getProductConfigFromDeviceName(acronym);
        EXPECT_EQ(ret, AOT::MTL_H_B0);
    }
}

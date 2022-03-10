/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpc_core/hw_cmds_base.h"
#include "shared/test/common/fixtures/product_config_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

#include "device_ids_configs_pvc.h"

using namespace NEO;

PVCTEST_F(ProductConfigTests, givenPvcXlDeviceIdWhenDifferentRevisionIsPassedThenCorrectProductConfigIsReturned) {
    for (const auto &deviceId : PVC_XL_IDS) {
        hwInfo.platform.usDeviceID = deviceId;

        hwInfo.platform.usRevId = 0x0;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, PVC_XL_A0);

        hwInfo.platform.usRevId = 0x1;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, PVC_XL_B0);

        hwInfo.platform.usRevId = 0x6;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, PVC_XL_B0);
    }
}

PVCTEST_F(ProductConfigTests, givenPvcXtDeviceIdWhenDifferentRevisionIsPassedThenCorrectProductConfigIsReturned) {
    for (const auto &deviceId : PVC_XT_IDS) {
        hwInfo.platform.usDeviceID = deviceId;

        hwInfo.platform.usRevId = 0x3;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, PVC_XT_A0);

        hwInfo.platform.usRevId = 0x5;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, PVC_XT_B0);

        hwInfo.platform.usRevId = 0x7;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, PVC_XT_B0);
    }
}

PVCTEST_F(ProductConfigTests, givenDefaultDeviceAndRevisionIdWhenGetProductConfigThenPvcXtA0ConfigIsReturned) {
    hwInfo.platform.usRevId = 0x0;
    hwInfo.platform.usDeviceID = 0x0;

    productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
    EXPECT_EQ(productConfig, PVC_XT_A0);
}

PVCTEST_F(ProductConfigTests, givenInvalidRevisionIdWhenGetProductConfigThenUnknownIsaIsReturned) {
    hwInfo.platform.usRevId = 0x2;
    productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
    EXPECT_EQ(productConfig, UNKNOWN_ISA);
}
/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/product_config_fixture.h"

#include "hw_cmds_xe_hpc_core_base.h"

using namespace NEO;

PVCTEST_F(ProductConfigTests, givenPvcXlDeviceIdWhenDifferentRevisionIsPassedThenCorrectProductConfigIsReturned) {
    for (const auto &deviceId : pvcXlDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;

        hwInfo.platform.usRevId = 0x0;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XL_A0);

        hwInfo.platform.usRevId = 0x1;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XL_A0P);

        hwInfo.platform.usRevId = 0x6;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XL_A0P);
    }
}

PVCTEST_F(ProductConfigTests, givenPvcXtDeviceIdWhenDifferentRevisionIsPassedThenCorrectProductConfigIsReturned) {
    for (const auto &deviceId : pvcXtDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;

        hwInfo.platform.usRevId = 0x3;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XT_A0);

        hwInfo.platform.usRevId = 0x5;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XT_B0);

        hwInfo.platform.usRevId = 0x6;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XT_B1);

        hwInfo.platform.usRevId = 0x7;
        productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
        EXPECT_EQ(productConfig, AOT::PVC_XT_C0);
    }
}

PVCTEST_F(ProductConfigTests, givenDefaultDeviceAndRevisionIdWhenGetProductConfigThenPvcXtC0ConfigIsReturned) {
    hwInfo.platform.usRevId = 0x0;
    hwInfo.platform.usDeviceID = 0x0;

    productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
    EXPECT_EQ(productConfig, AOT::PVC_XT_C0);
}

PVCTEST_F(ProductConfigTests, givenInvalidRevisionIdWhenGetProductConfigThenUnknownIsaIsReturned) {
    hwInfo.platform.usRevId = 0x2;
    productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
    EXPECT_EQ(productConfig, AOT::UNKNOWN_ISA);

    hwInfo.platform.usRevId = 0x4;
    productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
    EXPECT_EQ(productConfig, AOT::UNKNOWN_ISA);
}

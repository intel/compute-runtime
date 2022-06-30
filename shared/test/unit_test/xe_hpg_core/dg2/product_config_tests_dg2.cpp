/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"
#include "shared/test/common/fixtures/product_config_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

#include "device_ids_configs_dg2.h"

using namespace NEO;

DG2TEST_F(ProductConfigTests, givenDefaultDeviceAndRevisionIdWhenGetProductConfigThenDg2G11A0ConfigIsReturned) {
    hwInfo.platform.usRevId = 0x0;
    hwInfo.platform.usDeviceID = 0x0;

    productConfig = hwInfoConfig->getProductConfigFromHwInfo(hwInfo);
    EXPECT_EQ(productConfig, AOT::DG2_G11_A0);
}
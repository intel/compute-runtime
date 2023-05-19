/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

TEST_F(WddmTest, WhenPopulateIpVersionWddmIsCalledThenIpVersionIsSet) {

    auto &productHelper = wddm->rootDeviceEnvironment.getHelper<ProductHelper>();
    HardwareInfo hwInfo = *defaultHwInfo;
    auto config = productHelper.getProductConfigFromHwInfo(hwInfo);
    wddm->populateIpVersion(hwInfo);

    EXPECT_EQ(config, hwInfo.ipVersion.value);
}

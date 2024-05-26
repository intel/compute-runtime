/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

TEST_F(WddmTest, WhenPopulateIpVersionWddmIsCalledThenIpVersionIsSet) {

    auto &compilerProductHelper = wddm->rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    HardwareInfo hwInfo = *defaultHwInfo;
    auto config = compilerProductHelper.getHwIpVersion(hwInfo);
    wddm->populateIpVersion(hwInfo);

    EXPECT_EQ(config, hwInfo.ipVersion.value);
}

HWTEST2_F(WddmTest, WhenPopulateIpVersionWddmIsCalledAndIpVersionIsZeroThenDefaultValueIsSet, IsAtLeastMtl) {
    auto &compilerProductHelper = wddm->rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.ipVersion = 0;
    hwInfo.platform.usDeviceID = 0;
    auto config = compilerProductHelper.getDefaultHwIpVersion();
    wddm->populateIpVersion(hwInfo);

    EXPECT_EQ(config, hwInfo.ipVersion.value);
}

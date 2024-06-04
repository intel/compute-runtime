/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "platforms.h"

using namespace NEO;

using MtlProductHelper = ProductHelperTest;

MTLTEST_F(MtlProductHelper, givenProductHelperWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isDirectSubmissionSupported(releaseHelper));
}

MTLTEST_F(MtlProductHelper, givenMtlWithoutHwIpVersionInHwInfoWhenGettingIpVersionThenCorrectValueIsReturnedBasedOnDeviceIdAndRevId) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.ipVersion = {};

    auto mtlMDeviceIds = {0x7D40, 0x7D45};
    auto mtlPDeviceIds = {0x7D55, 0X7DD5};

    for (auto &deviceId : mtlMDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        for (auto &revision : {0, 2}) {
            hwInfo.platform.usRevId = revision;

            EXPECT_EQ(AOT::MTL_U_A0, compilerProductHelper->getHwIpVersion(hwInfo));
        }
        for (auto &revision : {3, 8}) {
            hwInfo.platform.usRevId = revision;

            EXPECT_EQ(AOT::MTL_U_B0, compilerProductHelper->getHwIpVersion(hwInfo));
        }
        hwInfo.platform.usRevId = 0xdead;

        EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), compilerProductHelper->getHwIpVersion(hwInfo));
    }

    for (auto &deviceId : mtlPDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        for (auto &revision : {0, 2}) {
            hwInfo.platform.usRevId = revision;

            EXPECT_EQ(AOT::MTL_H_A0, compilerProductHelper->getHwIpVersion(hwInfo));
        }
        for (auto &revision : {3, 8}) {
            hwInfo.platform.usRevId = revision;

            EXPECT_EQ(AOT::MTL_H_B0, compilerProductHelper->getHwIpVersion(hwInfo));
        }
        hwInfo.platform.usRevId = 0xdead;

        EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), compilerProductHelper->getHwIpVersion(hwInfo));
    }

    hwInfo.platform.usDeviceID = 0;
    hwInfo.platform.usRevId = 0xdead;

    EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), compilerProductHelper->getHwIpVersion(hwInfo));
}

MTLTEST_F(MtlProductHelper, givenProductHelperWhenCheckOverrideAllocationCacheableThenTrueIsReturnedForCommandBuffer) {
    AllocationData allocationData{};
    allocationData.type = AllocationType::commandBuffer;
    EXPECT_TRUE(productHelper->overrideAllocationCacheable(allocationData));

    allocationData.type = AllocationType::buffer;
    EXPECT_FALSE(productHelper->overrideAllocationCacheable(allocationData));
}

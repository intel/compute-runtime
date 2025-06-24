/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "neo_aot_platforms.h"

namespace NEO {
extern ApiSpecificConfig::ApiType apiTypeForUlts;
}

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

MTLTEST_F(MtlProductHelper, givenProductHelperWhenCheckoverrideAllocationCpuCacheableThenTrueIsReturnedForCommandBuffer) {
    AllocationData allocationData{};
    allocationData.type = AllocationType::commandBuffer;
    EXPECT_TRUE(productHelper->overrideAllocationCpuCacheable(allocationData));

    allocationData.type = AllocationType::buffer;
    EXPECT_FALSE(productHelper->overrideAllocationCpuCacheable(allocationData));
}

MTLTEST_F(MtlProductHelper, givenProductHelperWhenCheckingIsHostDeviceUsmPoolAllocatorSupportedThenCorrectValueIsReturned) {
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
        EXPECT_TRUE(productHelper->isHostUsmPoolAllocatorSupported());
        EXPECT_TRUE(productHelper->isDeviceUsmPoolAllocatorSupported());
    }
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
        EXPECT_FALSE(productHelper->isHostUsmPoolAllocatorSupported());
        EXPECT_FALSE(productHelper->isDeviceUsmPoolAllocatorSupported());
    }
}

MTLTEST_F(MtlProductHelper, givenProductHelperWhenCheckingIsUsmAllocationReuseSupportedThenCorrectValueIsReturned) {
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
        EXPECT_TRUE(productHelper->isHostUsmAllocationReuseSupported());
        EXPECT_TRUE(productHelper->isDeviceUsmAllocationReuseSupported());
    }
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
        EXPECT_FALSE(productHelper->isHostUsmAllocationReuseSupported());
        EXPECT_FALSE(productHelper->isDeviceUsmAllocationReuseSupported());
    }
}
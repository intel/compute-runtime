/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "opencl/source/platform/platform_info.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory.h"

using namespace NEO;

using MtlDeviceCapsTest = ::testing::Test;

MTLTEST_F(MtlDeviceCapsTest, whenCheckingExtensionThenCorrectExtensionsAreReported) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    unsigned int gmdReleases[] = {70, 71, 72, 73};
    defaultHwInfo->ipVersion.architecture = 12;

    for (auto gmdRelease : gmdReleases) {
        defaultHwInfo->ipVersion.release = gmdRelease;
        UltClDeviceFactory deviceFactory{1, 0};
        auto &extensions = deviceFactory.rootDevices[0]->deviceExtensions;

        EXPECT_EQ(!MTL::isLpg(*defaultHwInfo), hasSubstr(extensions, std::string("cl_intel_bfloat16_conversions")));
        EXPECT_EQ(!MTL::isLpg(*defaultHwInfo), hasSubstr(extensions, std::string("cl_intel_subgroup_matrix_multiply_accumulate")));
        EXPECT_EQ(!MTL::isLpg(*defaultHwInfo), hasSubstr(extensions, std::string("cl_intel_subgroup_split_matrix_multiply_accumulate")));
    }
}

using MtlPlatformCaps = Test<PlatformFixture>;

MTLTEST_F(MtlPlatformCaps, givenSkuWhenCheckingExtensionThenFp64IsReported) {
    const auto &caps = pPlatform->getPlatformInfo();

    EXPECT_NE(std::string::npos, caps.extensions.find(std::string("cl_khr_fp64")));
}

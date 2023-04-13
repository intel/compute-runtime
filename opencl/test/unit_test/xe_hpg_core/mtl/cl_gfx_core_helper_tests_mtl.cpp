/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_gfx_core_helper.h"

using namespace NEO;

using ClGfxCoreHelperTestsMtl = Test<ClDeviceFixture>;

MTLTEST_F(ClGfxCoreHelperTestsMtl, givenVariousMtlReleasesWhenGettingSupportedDeviceFeatureCapabilitiesThenReturnCorrectValue) {
    auto &hwInfo = *getRootDeviceEnvironment().getMutableHardwareInfo();
    unsigned int releases[] = {70, 71};
    hwInfo.ipVersion.architecture = 12;

    cl_device_feature_capabilities_intel expectedCapabilitiesIfNotLpg = CL_DEVICE_FEATURE_FLAG_DPAS_INTEL | CL_DEVICE_FEATURE_FLAG_DP4A_INTEL;
    cl_device_feature_capabilities_intel expectedgCapabilitiesIfLpg = CL_DEVICE_FEATURE_FLAG_DP4A_INTEL;
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();

    for (auto release : releases) {
        hwInfo.ipVersion.release = release;
        getMutableRootDeviceEnvironment().releaseHelper = ReleaseHelper::create(hwInfo.ipVersion);
        EXPECT_EQ(MTL::isLpg(hwInfo) ? expectedgCapabilitiesIfLpg : expectedCapabilitiesIfNotLpg, clGfxCoreHelper.getSupportedDeviceFeatureCapabilities(getRootDeviceEnvironment()));
    }
}

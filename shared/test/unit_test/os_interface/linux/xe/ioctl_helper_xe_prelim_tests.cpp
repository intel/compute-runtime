/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe_prelim.h"
#include "shared/source/os_interface/linux/xe/xedrm_prelim.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(IoctlHelperXePrelimTest, givenSimd16EuPerDssTypeWhenCheckingIfTopologyIsEuPerDssThenSuccessIsReturned) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};
    IoctlHelperXePrelim ioctlHelper{*drm};
    EXPECT_TRUE(ioctlHelper.isEuPerDssTopologyType(DRM_XE_TOPO_SIMD16_EU_PER_DSS));
    EXPECT_TRUE(ioctlHelper.isEuPerDssTopologyType(DRM_XE_TOPO_EU_PER_DSS));
    EXPECT_FALSE(ioctlHelper.isEuPerDssTopologyType(DRM_XE_TOPO_DSS_GEOMETRY));
    EXPECT_FALSE(ioctlHelper.isEuPerDssTopologyType(DRM_XE_TOPO_DSS_COMPUTE));
}

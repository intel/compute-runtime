/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

using namespace NEO;

using Gen11ClDeviceCaps = Test<ClDeviceFixture>;

GEN11TEST_F(Gen11ClDeviceCaps, givenGen11WhenCheckExtensionsThenSubgroupLocalBlockIOIsSupported) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_subgroup_local_block_io")));
}

GEN11TEST_F(Gen11ClDeviceCaps, givenGen11WhenCheckExtensionsThenDeviceProperlyReportsClKhrSubgroupsExtension) {
    const auto &caps = pClDevice->getDeviceInfo();

    if (pClDevice->areOcl21FeaturesEnabled()) {
        EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_subgroups")));
    } else {
        EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_subgroups")));
    }
}

GEN11TEST_F(Gen11ClDeviceCaps, givenGen11WhenCheckingCapsThenDeviceDoesProperlyReportsIndependentForwardProgress) {
    const auto &caps = pClDevice->getDeviceInfo();

    if (pClDevice->areOcl21FeaturesEnabled()) {
        EXPECT_TRUE(caps.independentForwardProgress != 0);
    } else {
        EXPECT_FALSE(caps.independentForwardProgress != 0);
    }
}

/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

using namespace NEO;
using ClDeviceCapsTestXe3pLpg = Test<ClDeviceFixture>;

HWTEST2_F(ClDeviceCapsTestXe3pLpg, givenDeviceExtensionsWhenDeviceCapsInitializedThenAddProperExtensions, IsXe3pLpg) {
    const auto &dInfo = pClDevice->getDeviceInfo();
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_create_buffer_with_properties")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_subgroup_local_block_io")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_subgroup_matrix_multiply_accumulate")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_khr_subgroup_named_barrier")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_subgroup_extended_block_read")));
}

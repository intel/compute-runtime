/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_cri.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/helpers/gtest_helpers.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "per_product_test_definitions.h"

using namespace NEO;

using CriClDeviceIdTest = Test<ClDeviceFixture>;

CRITEST_F(CriClDeviceIdTest, givenDeviceExtensionsWhenDeviceCapsInitializedThenAddProperExtensions) {
    const auto &dInfo = pClDevice->getDeviceInfo();
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_create_buffer_with_properties")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_subgroup_local_block_io")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_subgroup_matrix_multiply_accumulate")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_khr_subgroup_named_barrier")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_subgroup_extended_block_read")));
}

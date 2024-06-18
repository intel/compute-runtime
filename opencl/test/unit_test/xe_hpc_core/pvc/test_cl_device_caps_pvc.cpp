/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

using namespace NEO;

using PvcClDeviceCapsTests = Test<ClDeviceFixture>;

PVCTEST_F(PvcClDeviceCapsTests, givenPvcProductWhenDeviceCapsInitializedThenAddPvcExtensions) {
    const auto &dInfo = pClDevice->getDeviceInfo();
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_create_buffer_with_properties")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_subgroup_local_block_io")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_subgroup_matrix_multiply_accumulate")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_subgroup_matrix_multiply_accumulate_tf32")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_khr_subgroup_named_barrier")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_subgroup_extended_block_read")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_subgroup_2d_block_io")));
    EXPECT_TRUE(hasSubstr(dInfo.deviceExtensions, std::string("cl_intel_subgroup_buffer_prefetch")));
}
